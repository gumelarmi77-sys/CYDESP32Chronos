package com.example.displayconnect.notifications

import android.app.Notification
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import android.util.Log
import com.example.displayconnect.DisplayConnectApp
import com.example.displayconnect.models.ConnectionState

/**
 * NotificationListenerService for forwarding WhatsApp & WhatsApp Business messages
 * (including incoming WhatsApp Voice/Video Calls) to the ESP32 CYD over BLE NUS.
 */
class WhatsAppNotificationListenerService : NotificationListenerService() {

    private var lastSignature: String = ""
    private var lastSentAtMs: Long = 0L

    override fun onListenerConnected() {
        super.onListenerConnected()
        MediaControlBridge.getInstance(applicationContext).attachSessionListener()
    }

    override fun onListenerDisconnected() {
        MediaControlBridge.getInstance(applicationContext).detachSessionListener()
        super.onListenerDisconnected()
    }

    override fun onNotificationPosted(sbn: StatusBarNotification?) {
        val statusBarNotification = sbn ?: return
        val pkg = statusBarNotification.packageName ?: return
        if (pkg !in WHATSAPP_PACKAGES) return

        val app = application as? DisplayConnectApp ?: return
        if (app.navClient.connectionState.value != ConnectionState.CONNECTED) return

        val notification = statusBarNotification.notification ?: return
        val isWhatsAppCall = notification.category == Notification.CATEGORY_CALL

        // Ignore grouped summary headers and background sync notices (except live incoming calls)
        val flags = notification.flags
        if (!isWhatsAppCall && (flags and Notification.FLAG_GROUP_SUMMARY) != 0) return
        if (!isWhatsAppCall && (flags and Notification.FLAG_ONGOING_EVENT) != 0) return

        val extras = notification.extras ?: return
        val rawTitle = extras.getCharSequence(Notification.EXTRA_TITLE)?.toString()?.trim().orEmpty()
        val rawBigText = extras.getCharSequence(Notification.EXTRA_BIG_TEXT)?.toString()?.trim().orEmpty()
        val rawText = extras.getCharSequence(Notification.EXTRA_TEXT)?.toString()?.trim().orEmpty()
        val messageBody = rawBigText.ifBlank { rawText }

        if (rawTitle.isBlank() && messageBody.isBlank()) return
        if (!isWhatsAppCall && isSystemSyncPlaceholder(rawTitle, messageBody)) return

        val sender = rawTitle.ifBlank { "WhatsApp" }
        val signature = "$pkg|$isWhatsAppCall|$sender|$messageBody"
        val now = System.currentTimeMillis()
        if (signature == lastSignature && (now - lastSentAtMs) < DEDUP_WINDOW_MS) {
            return
        }

        val jsonPayload = if (isWhatsAppCall) {
            NavMessageNotify.call(
                caller = sender,
                subtitle = messageBody.ifBlank { "Panggilan WhatsApp masuk" }
            )
        } else {
            NavMessageNotify.whatsapp(
                sender = sender,
                message = messageBody.ifBlank { "Pesan WhatsApp baru" }
            )
        }

        val sent = app.navClient.sendNavMessage(jsonPayload)
        if (sent) {
            lastSignature = signature
            lastSentAtMs = now
            Log.d(TAG, "Forwarded WhatsApp notify to CYD: $sender")
        }
    }

    override fun onNotificationRemoved(sbn: StatusBarNotification?) {
        val statusBarNotification = sbn ?: return
        val pkg = statusBarNotification.packageName ?: return
        if (pkg !in WHATSAPP_PACKAGES) return

        val notification = statusBarNotification.notification ?: return
        if (notification.category == Notification.CATEGORY_CALL) {
            val app = application as? DisplayConnectApp ?: return
            if (app.navClient.connectionState.value == ConnectionState.CONNECTED) {
                app.navClient.sendNavMessage(NavMessageNotify.clear())
            }
        }
    }

    private fun isSystemSyncPlaceholder(title: String, body: String): Boolean {
        val combined = "$title $body".lowercase()
        return combined.contains("checking for new messages") ||
            combined.contains("memeriksa pesan baru") ||
            combined.contains("whatsapp web") ||
            combined.contains("backup in progress") ||
            combined.contains("cadangan")
    }

    companion object {
        private const val TAG = "WaNotifyCYD"
        private const val DEDUP_WINDOW_MS = 2500L
        private val WHATSAPP_PACKAGES = setOf(
            "com.whatsapp",
            "com.whatsapp.w4b"
        )
    }
}
