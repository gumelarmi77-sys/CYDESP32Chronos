# Patch Android DisplayConnect — WhatsApp, Telepon Masuk & Kontrol Music Player ke ESP32 CYD

Patch ini dirancang khusus untuk repositori **malaq88/DisplayConnect** (`com.example.displayconnect`, Android SDK min 24 / target 36) agar **Notifikasi WhatsApp**, **Panggilan Masuk**, dan **Kontrol Music Player (Spotify, YouTube Music, Apple Music, dll)** bekerja dua arah melalui BLE NUS dengan **DisplayConnect Moto CYD (ESP32-2432S028R)**.

---

## Langkah Pemasangan di Android Studio

### 1. Salin 5 file Kotlin baru ke folder `notifications`
Salin folder `notifications/` dari paket ini ke dalam proyek Android Anda:
```
app/src/main/java/com/example/displayconnect/notifications/
├── NavMessageNotify.kt
├── WhatsAppNotificationListenerService.kt
├── IncomingCallMonitor.kt
├── MediaControlBridge.kt
└── NotificationPermissionHelper.kt
```

### 2. Perbarui `AndroidManifest.xml`
Tambahkan izin & `<service>` sesuai isi `AndroidManifest_PATCH.xml`:
- Permission: `READ_PHONE_STATE`, `READ_CALL_LOG`, (opsional) `READ_CONTACTS`
- Service: `.notifications.WhatsAppNotificationListenerService` dengan `BIND_NOTIFICATION_LISTENER_SERVICE`

### 3. Hubungkan `BleNavClient.kt`, `DisplayConnectApp.kt`, & `MainActivity.kt`
Lihat `DisplayConnectApp_PATCH.kt`:
- Di `DisplayConnectApp.kt`: jalankan `IncomingCallMonitor` dan `MediaControlBridge`.
- Di `BleNavClient.kt`: tambahkan `onCharacteristicChanged` pada `BluetoothGattCallback` agar perintah dari layar sentuh CYD (`{"type":"media_cmd","cmd":"next"}`) diterima dari `TX_UUID` (`6E400003-...`) dan langsung mengontrol Spotify / player musik HP.

### 4. Aktifkan Izin di HP Android
1. Build & install APK ke HP Anda.
2. Buka **Pengaturan HP → Aplikasi → Akses khusus → Akses notifikasi** → aktifkan **DisplayConnect** (dibutuhkan untuk membaca notifikasi WhatsApp & mengontrol `MediaSessionManager`).
3. Berikan izin **Telepon** saat diminta.

---

## Cara Pakai di Layar CYD ESP32
- Tap pojok **Kanan Atas** layar CYD untuk berganti mode: **`MAP`** (Peta Vektor) → **`HUD`** (Panah Besar Motor) → **`MUS`** (**Audio Deck / Music Player**).
- Di menu **`MUS`**, tersedia tombol layar sentuh ukuran sarung tangan:
  - **PREV (`|<`)**, **PLAY / PAUSE (`|>` / `||`)**, **NEXT (`>|`)**
  - **VOL -**, **STOP (`[]`)**, **VOL +**
- Judul lagu, artis, status Play/Pause, dan informasi belokan navigasi berikutnya tetap tampil di bagian bawah layar.
