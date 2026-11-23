=============================================================================
PANDUAN ARDUINO IDE — DisplayConnect Moto CYD (ESP32-2432S028R)
=============================================================================

Ya, folder "DisplayConnect_Moto_CYD" di dalam ZIP ini bisa langsung dibuka
di Arduino IDE! Ikuti 4 langkah singkat berikut agar langsung berhasil compile
& upload tanpa layar putih:

1. BUKA SKETCH DI ARDUINO IDE
   - Ekstrak file ZIP ini.
   - Buka file: DisplayConnect_Moto_CYD/DisplayConnect_Moto_CYD.ino
   - (JANGAN ubah nama folder "DisplayConnect_Moto_CYD" agar Arduino IDE
     otomatis memuat semua tab .cpp dan .h di dalamnya).

2. INSTALL LIBRARY DI ARDUINO IDE (Menu: Sketch > Include Library > Manage Libraries)
   - TFT_eSPI (by Bodmer)
   - NimBLE-Arduino (by h2zero) -> versi 2.1.2+
   - ChronosESP32 (by Felix Biego) -> WAJIB untuk koneksi ke aplikasi Chronos Android
   - ESP32Time (by Felix Biego / fbiego) -> dependensi ChronosESP32
   - XPT2046_Touchscreen (by Paul Stoffregen)
   - TinyGPSPlus (by Mikal Hart) -> jika ENABLE_HW_GPS = 1 di config.h
   (Catatan: ArduinoJson TIDAK diperlukan lagi — parser sudah built-in!)

3. KONFIGURASI TFT_eSPI (WAJIB UNTUK ARDUINO IDE!)
   Arduino IDE tidak otomatis membaca pin layar dari folder sketch.
   - Salin file "User_Setup_CYD.h" yang ada di folder sketch ini.
   - Buka folder: Documents/Arduino/libraries/TFT_eSPI/
   - Ganti (timpa/overwrite) file "User_Setup.h" dengan isi "User_Setup_CYD.h".
   - Catatan: Jika layar CYD Anda tipe 2-USB (ST7789) dan gambar terbalik/putih,
     pastikan di tab "Konfigurasi" web Anda memilih "ST7789 (Varian 2-USB)"
     sebelum Download .ZIP.

4. SETTING BOARD & UPLOAD
   - Tools > Board > ESP32 Arduino > "ESP32 Dev Module"
   - Tools > Partition Scheme > "Huge APP (3MB No OTA/1MB SPIFFS)"
     (Wajib agar NimBLE + TFT_eSPI cukup di flash memory!)
   - Tools > Upload Speed > 921600 (atau 460800 jika gagal)
   - Klik Upload!

-----------------------------------------------------------------------------
ALTERNATIF LEBIH PRAKTIS: PLATFORMIO (VS Code)
Jika memakai PlatformIO, Anda TIDAK perlu menyalin User_Setup.h secara manual
karena file "platformio.ini" sudah mengatur seluruh library dan pin TFT CYD.
-----------------------------------------------------------------------------
