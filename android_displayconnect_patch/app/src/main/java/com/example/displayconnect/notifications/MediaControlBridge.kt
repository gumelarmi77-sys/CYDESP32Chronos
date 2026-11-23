package com.example.displayconnect.notifications

import android.content.ComponentName
import android.content.Context
import android.media.AudioManager
import android.media.MediaMetadata
import android.media.session.MediaController
import android.media.session.MediaSessionManager
import android.media.session.PlaybackState
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import android.view.KeyEvent
import com.example.displayconnect.DisplayConnectApp
import com.example.displayconnect.models.ConnectionState
import org.json.JSONObject

/**
 * Bidirectional Android Music Player Bridge for DisplayConnect CYD.
 * - Executes media commands received from CYD touchscreen (prev, play_pause, next, stop, vol_up, vol_down)
 *   via MediaSessionManager + AudioManager KeyEvent fallback (works with Spotify, YT Music, Apple Music, etc.)
 * - Pushes active track title/artist/state back to CYD over BLE when song or play state changes.
 */
class MediaControlBridge private constructor(context: Context) {

    private val appContext = context.applicationContext
    private val audioManager = appContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private val mediaSessionManager =
        appContext.getSystemService(Context.MEDIA_SESSION_SERVICE) as? MediaSessionManager
    private val listenerComponent =
        ComponentName(appContext, WhatsAppNotificationListenerService::class.java)
    private val mainHandler = Handler(Looper.getMainLooper())

    private var activeController: MediaController? = null
    private var attached = false
    private var lastPushedJson: String = ""

    private val controllerCallback = object : MediaController.Callback() {
        override fun onMetadataChanged(metadata: MediaMetadata?) {
            pushCurrentMetadataToCyd(force = false)
        }

        override fun onPlaybackStateChanged(state: PlaybackState?) {
            pushCurrentMetadataToCyd(force = false)
        }
    }

    private val sessionsChangedListener =
        MediaSessionManager.OnActiveSessionsChangedListener { controllers ->
            bindPrimaryController(controllers)
        }

    fun attachSessionListener() {
        if (attached) {
            refreshActiveSession()
            return
        }
        try {
            val manager = mediaSessionManager ?: return
            manager.addOnActiveSessionsChangedListener(sessionsChangedListener, listenerComponent, mainHandler)
            attached = true
            refreshActiveSession()
        } catch (se: SecurityException) {
            Log.w(TAG, "Notification Access not yet granted for MediaSessionManager", se)
        }
    }

    fun detachSessionListener() {
        if (!attached) return
        try {
            mediaSessionManager?.removeOnActiveSessionsChangedListener(sessionsChangedListener)
        } catch (_: Exception) {
        }
        unbindController()
        attached = false
    }

    fun refreshActiveSession() {
        try {
            val controllers = mediaSessionManager?.getActiveSessions(listenerComponent)
            bindPrimaryController(controllers)
        } catch (_: SecurityException) {
            // Notification Access not granted yet
        }
    }

    /**
     * Parses BLE TX JSON lines sent from ESP32 CYD, e.g. {"type":"media_cmd","cmd":"next"}
     */
    fun onBleLineFromCyd(rawLine: String) {
        val trimmed = rawLine.trim()
        if (!trimmed.startsWith("{")) return
        try {
            val json = JSONObject(trimmed)
            if (json.optString("type") == "media_cmd") {
                val cmd = json.optString("cmd")
                if (cmd.isNotBlank()) {
                    mainHandler.post { handleCommand(cmd) }
                }
            }
        } catch (_: Exception) {
            // Ignore non-JSON status lines such as "OK"
        }
    }

    fun handleCommand(cmd: String) {
        Log.d(TAG, "Executing CYD media command: $cmd")
        when (cmd) {
            "vol_up" -> {
                audioManager.adjustStreamVolume(
                    AudioManager.STREAM_MUSIC,
                    AudioManager.ADJUST_RAISE,
                    AudioManager.FLAG_SHOW_UI
                )
                return
            }

            "vol_down" -> {
                audioManager.adjustStreamVolume(
                    AudioManager.STREAM_MUSIC,
                    AudioManager.ADJUST_LOWER,
                    AudioManager.FLAG_SHOW_UI
                )
                return
            }
        }

        val controller = getBestController()
        if (controller != null) {
            val controls = controller.transportControls
            val playbackState = controller.playbackState?.state
            when (cmd) {
                "play_pause" -> {
                    if (playbackState == PlaybackState.STATE_PLAYING ||
                        playbackState == PlaybackState.STATE_BUFFERING
                    ) {
                        controls.pause()
                    } else {
                        controls.play()
                    }
                }
                "play" -> controls.play()
                "pause" -> controls.pause()
                "stop" -> controls.stop()
                "next" -> controls.skipToNext()
                "prev" -> controls.skipToPrevious()
            }
            mainHandler.postDelayed({ pushCurrentMetadataToCyd(force = true) }, 220L)
        } else {
            val keyCode = when (cmd) {
                "play_pause" -> KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE
                "play" -> KeyEvent.KEYCODE_MEDIA_PLAY
                "pause" -> KeyEvent.KEYCODE_MEDIA_PAUSE
                "stop" -> KeyEvent.KEYCODE_MEDIA_STOP
                "next" -> KeyEvent.KEYCODE_MEDIA_NEXT
                "prev" -> KeyEvent.KEYCODE_MEDIA_PREVIOUS
                else -> return
            }
            dispatchMediaKey(keyCode)
        }
    }

    fun pushCurrentMetadataToCyd(force: Boolean = false) {
        val app = appContext as? DisplayConnectApp ?: return
        if (app.navClient.connectionState.value != ConnectionState.CONNECTED) return

        val controller = getBestController() ?: return
        val metadata = controller.metadata
        val title = metadata?.getString(MediaMetadata.METADATA_KEY_TITLE)
            ?: metadata?.getString(MediaMetadata.METADATA_KEY_DISPLAY_TITLE)
            ?: "Audio Aktif"
        val artist = metadata?.getString(MediaMetadata.METADATA_KEY_ARTIST)
            ?: metadata?.getString(MediaMetadata.METADATA_KEY_ALBUM_ARTIST)
            ?: controller.packageName
        val appLabel = resolvePlayerLabel(controller.packageName)

        val pbState = controller.playbackState?.state
        val stateStr = when (pbState) {
            PlaybackState.STATE_PLAYING,
            PlaybackState.STATE_BUFFERING -> "playing"
            PlaybackState.STATE_PAUSED -> "paused"
            else -> "stopped"
        }

        val json = NavMessageNotify.media(stateStr, title, artist, appLabel)
        if (!force && json == lastPushedJson) return
        if (app.navClient.sendNavMessage(json)) {
            lastPushedJson = json
        }
    }

    private fun getBestController(): MediaController? {
        if (activeController != null) return activeController
        refreshActiveSession()
        return activeController
    }

    private fun bindPrimaryController(controllers: List<MediaController>?) {
        val chosen = controllers?.firstOrNull {
            it.playbackState?.state == PlaybackState.STATE_PLAYING
        } ?: controllers?.firstOrNull()

        if (chosen?.sessionToken == activeController?.sessionToken) return

        unbindController()
        activeController = chosen
        chosen?.registerCallback(controllerCallback, mainHandler)
        pushCurrentMetadataToCyd(force = true)
    }

    private fun unbindController() {
        try {
            activeController?.unregisterCallback(controllerCallback)
        } catch (_: Exception) {
        }
        activeController = null
    }

    private fun dispatchMediaKey(keyCode: Int) {
        val now = SystemClock.uptimeMillis()
        audioManager.dispatchMediaKeyEvent(KeyEvent(now, now, KeyEvent.ACTION_DOWN, keyCode, 0))
        audioManager.dispatchMediaKeyEvent(KeyEvent(now, now, KeyEvent.ACTION_UP, keyCode, 0))
    }

    private fun resolvePlayerLabel(pkg: String): String = when {
        pkg.contains("spotify", ignoreCase = true) -> "SPOTIFY"
        pkg.contains("youtube.music", ignoreCase = true) -> "YT MUSIC"
        pkg.contains("youtube", ignoreCase = true) -> "YOUTUBE"
        pkg.contains("apple", ignoreCase = true) -> "APPLE MUS"
        pkg.contains("joox", ignoreCase = true) -> "JOOX"
        pkg.contains("resso", ignoreCase = true) -> "RESSO"
        else -> pkg.substringAfterLast('.').uppercase().take(12)
    }

    companion object {
        private const val TAG = "MediaBridgeCYD"

        @Volatile
        private var instance: MediaControlBridge? = null

        fun getInstance(context: Context): MediaControlBridge {
            return instance ?: synchronized(this) {
                instance ?: MediaControlBridge(context).also { instance = it }
            }
        }
    }
}
