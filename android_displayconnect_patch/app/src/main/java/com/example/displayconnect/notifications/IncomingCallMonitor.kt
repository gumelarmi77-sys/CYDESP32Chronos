package com.example.displayconnect.notifications

import android.Manifest
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.database.Cursor
import android.net.Uri
import android.os.Build
import android.provider.ContactsContract
import android.telephony.PhoneStateListener
import android.telephony.TelephonyCallback
import android.telephony.TelephonyManager
import android.util.Log
import androidx.annotation.RequiresApi
import androidx.core.content.ContextCompat
import com.example.displayconnect.models.ConnectionState
import com.example.displayconnect.network.BleNavClient

/**
 * Monitors GSM incoming calls via TelephonyCallback (Android 12+) and BroadcastReceiver
 * fallback (Android 7-11), forwarding caller name/number to the ESP32 CYD over BLE.
 */
class IncomingCallMonitor(
    private val context: Context,
    private val navClient: BleNavClient
) {
    private val appContext = context.applicationContext
    private val telephonyManager =
        appContext.getSystemService(Context.TELEPHONY_SERVICE) as? TelephonyManager

    private var started = false
    private var wasRinging = false
    private var lastIncomingNumber: String? = null

    private var callStateCallback: Any? = null

    @Suppress("DEPRECATION")
    private var legacyListener: PhoneStateListener? = null

    private val phoneStateReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context?, intent: Intent?) {
            if (intent?.action != TelephonyManager.ACTION_PHONE_STATE_CHANGED) return
            val stateStr = intent.getStringExtra(TelephonyManager.EXTRA_STATE) ?: return
            @Suppress("DEPRECATION")
            val number = intent.getStringExtra(TelephonyManager.EXTRA_INCOMING_NUMBER)
            if (!number.isNullOrBlank()) {
                lastIncomingNumber = number
            }
            val mappedState = when (stateStr) {
                TelephonyManager.EXTRA_STATE_RINGING -> TelephonyManager.CALL_STATE_RINGING
                TelephonyManager.EXTRA_STATE_OFFHOOK -> TelephonyManager.CALL_STATE_OFFHOOK
                else -> TelephonyManager.CALL_STATE_IDLE
            }
            handleCallStateChange(mappedState, lastIncomingNumber)
        }
    }

    fun start() {
        if (started) return
        if (!hasPermission(Manifest.permission.READ_PHONE_STATE)) {
            Log.w(TAG, "READ_PHONE_STATE not granted; call forwarding inactive")
            return
        }

        started = true
        val filter = IntentFilter(TelephonyManager.ACTION_PHONE_STATE_CHANGED)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            appContext.registerReceiver(phoneStateReceiver, filter, Context.RECEIVER_EXPORTED)
        } else {
            @Suppress("UnspecifiedRegisterReceiverFlag")
            appContext.registerReceiver(phoneStateReceiver, filter)
        }

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                registerApi31Callback()
            } else {
                registerLegacyListener()
            }
        } catch (se: SecurityException) {
            Log.w(TAG, "SecurityException registering call listener", se)
        }
    }

    fun stop() {
        if (!started) return
        started = false
        try {
            appContext.unregisterReceiver(phoneStateReceiver)
        } catch (_: Exception) {
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val cb = callStateCallback as? TelephonyCallback
            if (cb != null) {
                try {
                    telephonyManager?.unregisterTelephonyCallback(cb)
                } catch (_: Exception) {
                }
            }
            callStateCallback = null
        } else {
            @Suppress("DEPRECATION")
            val listener = legacyListener
            if (listener != null) {
                try {
                    @Suppress("DEPRECATION")
                    telephonyManager?.listen(listener, PhoneStateListener.LISTEN_NONE)
                } catch (_: Exception) {
                }
            }
            legacyListener = null
        }
    }

    @RequiresApi(Build.VERSION_CODES.S)
    private fun registerApi31Callback() {
        val callback = object : TelephonyCallback(), TelephonyCallback.CallStateListener {
            override fun onCallStateChanged(state: Int) {
                handleCallStateChange(state, lastIncomingNumber)
            }
        }
        callStateCallback = callback
        telephonyManager?.registerTelephonyCallback(appContext.mainExecutor, callback)
    }

    @Suppress("DEPRECATION")
    private fun registerLegacyListener() {
        val listener = object : PhoneStateListener() {
            @Deprecated("Deprecated in Java")
            override fun onCallStateChanged(state: Int, phoneNumber: String?) {
                if (!phoneNumber.isNullOrBlank()) {
                    lastIncomingNumber = phoneNumber
                }
                handleCallStateChange(state, lastIncomingNumber)
            }
        }
        legacyListener = listener
        telephonyManager?.listen(listener, PhoneStateListener.LISTEN_CALL_STATE)
    }

    private fun handleCallStateChange(state: Int, phoneNumber: String?) {
        if (navClient.connectionState.value != ConnectionState.CONNECTED) {
            if (state == TelephonyManager.CALL_STATE_IDLE) {
                wasRinging = false
                lastIncomingNumber = null
            }
            return
        }

        when (state) {
            TelephonyManager.CALL_STATE_RINGING -> {
                wasRinging = true
                val contactName = resolveContactName(phoneNumber)
                val callerLabel = when {
                    !contactName.isNullOrBlank() -> contactName
                    !phoneNumber.isNullOrBlank() -> phoneNumber
                    else -> "Panggilan Telepon"
                }
                val subtitle = if (!contactName.isNullOrBlank() && !phoneNumber.isNullOrBlank()) {
                    phoneNumber
                } else {
                    "Telepon masuk - tap CYD untuk tutup"
                }
                navClient.sendNavMessage(NavMessageNotify.call(callerLabel, subtitle))
            }

            TelephonyManager.CALL_STATE_OFFHOOK,
            TelephonyManager.CALL_STATE_IDLE -> {
                if (wasRinging) {
                    wasRinging = false
                    lastIncomingNumber = null
                    navClient.sendNavMessage(NavMessageNotify.clear())
                }
            }
        }
    }

    private fun resolveContactName(phoneNumber: String?): String? {
        if (phoneNumber.isNullOrBlank()) return null
        if (!hasPermission(Manifest.permission.READ_CONTACTS)) return null
        val uri = Uri.withAppendedPath(
            ContactsContract.PhoneLookup.CONTENT_FILTER_URI,
            Uri.encode(phoneNumber)
        )
        var cursor: Cursor? = null
        return try {
            cursor = appContext.contentResolver.query(
                uri,
                arrayOf(ContactsContract.PhoneLookup.DISPLAY_NAME),
                null,
                null,
                null
            )
            if (cursor != null && cursor.moveToFirst()) {
                cursor.getString(0)
            } else {
                null
            }
        } catch (_: Exception) {
            null
        } finally {
            cursor?.close()
        }
    }

    private fun hasPermission(permission: String): Boolean {
        return ContextCompat.checkSelfPermission(appContext, permission) ==
            PackageManager.PERMISSION_GRANTED
    }

    companion object {
        private const val TAG = "CallMonitorCYD"
    }
}
