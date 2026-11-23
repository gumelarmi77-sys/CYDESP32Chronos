#pragma once

// =============================================================================
// DisplayConnect Motorcycle CYD (ESP32-2432S028R) Configuration
// Compatible with https://github.com/malaq88/DisplayConnect (BLE NUS v3.0)
// =============================================================================

// 1 = Kompatibel langsung dengan aplikasi Android Chronos (fbiego/chronos-esp32)
// 0 = Mode DisplayConnect BLE JSON standar (Web BLE Tester / DisplayConnect APK)
#define ENABLE_CHRONOS_APP 1

// BLE device name shown in smartphone Chronos / DisplayConnect scan list
#define BLE_DEVICE_NAME "DisplayConnect-CYD"

// Nordic UART Service (NUS) UUIDs — dipakai bersama Chronos & DisplayConnect
#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_MTU 256

// Default language: "id" (Indonesia), "en" (English), or "pt-BR"
#define DEFAULT_LANG "id"

// Screen rotation: 0 = Portrait (240x320, standard DisplayConnect), 1 = Landscape (320x240)
// Note: DisplayConnect Android app projects map pixels to 240x232 in Portrait (rotation 0).
#define TFT_ROTATION 0

// Backlight PWM (GPIO 21 on ESP32-2432S028R)
#define TFT_BL_PIN 21
#define BL_PWM_CHANNEL 0
#define BL_PWM_FREQ 5000
#define BL_PWM_RES 8

// Touch Controller XPT2046 (Dedicated VSPI bus on CYD)
#define TOUCH_CS_PIN   33
#define TOUCH_IRQ_PIN  36
#define TOUCH_MOSI_PIN 32
#define TOUCH_MISO_PIN 39
#define TOUCH_CLK_PIN  25

// Optional Hardware GPS Module (NEO-6M / NEO-8M) via CYD CN1 / P3 JST connector
// Set to 1 if wiring physical NEO-6M GPS to GPIO 27 (RX) & GPIO 22 (TX)
#define ENABLE_HW_GPS 1
#define GPS_RX_PIN    27
#define GPS_TX_PIN    22
#define GPS_BAUD      9600

// Optional Piezo Buzzer / Turn Alert Speaker (CYD Speaker/Audio Pin GPIO 26)
#define ENABLE_TURN_BUZZER 1
#define BUZZER_PIN         26
#define TURN_ALERT_DIST_M  100

// Optional LDR Ambient Light Sensor on CYD (GPIO 34) for auto night dimming
#define ENABLE_AUTO_DIMMING 0
#define LDR_PIN             34
