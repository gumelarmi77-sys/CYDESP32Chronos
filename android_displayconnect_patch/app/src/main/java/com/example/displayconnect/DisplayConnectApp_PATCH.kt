// =============================================================================
// Panduan Integrasi ke DisplayConnectApp.kt, MainActivity.kt & BleNavClient.kt
// Repository: https://github.com/malaq88/DisplayConnect
// =============================================================================

// --- 1. Di DisplayConnectApp.kt ---
package com.example.displayconnect

import android.app.Application
import com.example.displayconnect.network.BleNavClient
import com.example.displayconnect.notifications.IncomingCallMonitor
import com.example.displayconnect.notifications.MediaControlBridge
import com.example.displayconnect.storage.SettingsRepository

class DisplayConnectApp : Application() {
    lateinit var settingsRepository: SettingsRepository
        private set
    lateinit var navClient: BleNavClient
        private set
    lateinit var incomingCallMonitor: IncomingCallMonitor
        private set
    lateinit var mediaControlBridge: MediaControlBridge
        private set

    override fun onCreate() {
        super.onCreate()
        com.example.displayconnect.utils.AppLanguage.load(this)
        settingsRepository = SettingsRepository(this)
        navClient = BleNavClient(this)
        incomingCallMonitor = IncomingCallMonitor(this, navClient)
        incomingCallMonitor.start()

        mediaControlBridge = MediaControlBridge.getInstance(this)
        mediaControlBridge.attachSessionListener()
    }

    override fun onTerminate() {
        mediaControlBridge.detachSessionListener()
        incomingCallMonitor.stop()
        navClient.release()
        super.onTerminate()
    }
}

// --- 2. Di BleNavClient.kt (di dalam BluetoothGattCallback) ---
// Tambahkan callback onCharacteristicChanged agar perintah tombol Music Player
// dari layar sentuh ESP32 CYD (TX_UUID) langsung dieksekusi di HP:
/*
private val txBuffer = StringBuilder()

override fun onCharacteristicChanged(
    gatt: BluetoothGatt,
    characteristic: BluetoothGattCharacteristic,
    value: ByteArray
) {
    handleTxBytes(characteristic, value)
}

@Deprecated("Deprecated in Java")
override fun onCharacteristicChanged(
    gatt: BluetoothGatt,
    characteristic: BluetoothGattCharacteristic
) {
    @Suppress("DEPRECATION")
    val bytes = characteristic.value ?: return
    handleTxBytes(characteristic, bytes)
}

private fun handleTxBytes(characteristic: BluetoothGattCharacteristic, bytes: ByteArray) {
    if (characteristic.uuid != TX_UUID) return
    txBuffer.append(bytes.toString(Charsets.UTF_8))
    while (true) {
        val idx = txBuffer.indexOf("\n")
        if (idx < 0) break
        val line = txBuffer.substring(0, idx).trim()
        txBuffer.delete(0, idx + 1)
        if (line.isNotEmpty()) {
            com.example.displayconnect.notifications.MediaControlBridge
                .getInstance(appContext)
                .onBleLineFromCyd(line)
        }
    }
}
*/

// --- 3. Di MainActivity.kt (launcher izin telepon & Akses Notifikasi) ---
/*
import com.example.displayconnect.notifications.NotificationPermissionHelper

private val callPermissionLauncher = registerForActivityResult(
    ActivityResultContracts.RequestMultiplePermissions()
) { grants ->
    if (grants[Manifest.permission.READ_PHONE_STATE] == true) {
        (application as DisplayConnectApp).incomingCallMonitor.start()
    }
}

// Panggil di onCreate() setelah requestNotificationPermissionIfNeeded():
if (!NotificationPermissionHelper.hasCallPermissions(this)) {
    callPermissionLauncher.launch(NotificationPermissionHelper.requiredCallPermissions())
} else {
    (application as DisplayConnectApp).incomingCallMonitor.start()
}

// Opsional: di SettingsScreen.kt, tambahkan tombol Compose untuk membuka Akses Notifikasi:
if (!NotificationPermissionHelper.isNotificationListenerEnabled(context)) {
    Button(onClick = { NotificationPermissionHelper.openNotificationListenerSettings(context) }) {
        Text("Aktifkan Akses Notifikasi & Kontrol Musik CYD")
    }
}
*/
