import JSZip from 'jszip';
import { CYDHardwareConfig, FirmwareFile } from '../types';

export function buildCustomConfigH(cfg: CYDHardwareConfig): string {
  return `#pragma once

// =============================================================================
// DisplayConnect Motorcycle CYD (ESP32-2432S028R) Configuration
// Compatible with https://github.com/malaq88/DisplayConnect (BLE NUS v3.0)
// =============================================================================

// 1 = Kompatibel langsung dengan aplikasi Android Chronos (fbiego/chronos-esp32)
// 0 = Mode DisplayConnect BLE JSON standar (Web BLE Tester / DisplayConnect APK)
#define ENABLE_CHRONOS_APP ${cfg.enableChronosApp ? 1 : 0}

// BLE device name shown in smartphone Chronos / DisplayConnect scan list
#define BLE_DEVICE_NAME "${cfg.bleDeviceName.replace(/"/g, '')}"

// Nordic UART Service (NUS) UUIDs — dipakai bersama Chronos & DisplayConnect
#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_MTU 256

// Default language: "id" (Indonesia), "en" (English), or "pt-BR"
#define DEFAULT_LANG "${cfg.defaultLang}"

// Screen rotation: 0 = Portrait (240x320, standard DisplayConnect), 1 = Landscape (320x240)
#define TFT_ROTATION ${cfg.rotation}

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
#define ENABLE_HW_GPS ${cfg.enableHwGps ? 1 : 0}
#define GPS_RX_PIN    ${cfg.gpsRxPin}
#define GPS_TX_PIN    ${cfg.gpsTxPin}
#define GPS_BAUD      ${cfg.gpsBaud}

// Optional Piezo Buzzer / Turn Alert Speaker (CYD Speaker/Audio Pin GPIO 26)
#define ENABLE_TURN_BUZZER ${cfg.enableTurnBuzzer ? 1 : 0}
#define BUZZER_PIN         ${cfg.buzzerPin}
#define TURN_ALERT_DIST_M  ${cfg.turnAlertDistM}

// Optional LDR Ambient Light Sensor on CYD (GPIO 34)
#define ENABLE_AUTO_DIMMING 0
#define LDR_PIN             34
`;
}

export function buildCustomUserSetupH(cfg: CYDHardwareConfig): string {
  const isSt7789 = cfg.tftDriver === 'ST7789_DRIVER';
  return `// =============================================================================
// TFT_eSPI User_Setup.h for ESP32-2432S028R (Cheap Yellow Display / CYD)
// Copy this to Arduino/libraries/TFT_eSPI/User_Setup.h (if using Arduino IDE)
// =============================================================================

#define USER_SETUP_INFO "ESP32_CYD_2432S028R"

${isSt7789 ? '// #define ILI9341_2_DRIVER\n#define ST7789_DRIVER\n#define TFT_RGB_ORDER TFT_BGR' : '#define ILI9341_2_DRIVER\n// #define ST7789_DRIVER\n// #define TFT_RGB_ORDER TFT_BGR'}

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

#define USE_HSPI_PORT

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

// Touch CS is handled separately via XPT2046_Touchscreen on VSPI (CS=33)
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY       55000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
`;
}

export function buildCustomPlatformioIni(cfg: CYDHardwareConfig): string {
  const driverFlag =
    cfg.tftDriver === 'ST7789_DRIVER'
      ? '-DST7789_DRIVER=1\n  -DTFT_RGB_ORDER=TFT_BGR'
      : '-DILI9341_2_DRIVER=1';

  return `[platformio]
src_dir = DisplayConnect_Moto_CYD

[env:esp32-cyd-moto]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600
board_build.partitions = huge_app.csv

lib_deps =
  bodmer/TFT_eSPI@^2.5.43
  h2zero/NimBLE-Arduino@^2.1.2
  fbiego/ChronosESP32@^1.9.1
  paulstoffregen/XPT2046_Touchscreen@^1.4
  mikalhart/TinyGPSPlus@^1.1.0

build_flags =
  -DUSER_SETUP_LOADED=1
  ${driverFlag}
  -DTFT_WIDTH=240
  -DTFT_HEIGHT=320
  -DTFT_BL=21
  -DTFT_BACKLIGHT_ON=HIGH
  -DUSE_HSPI_PORT=1
  -DTFT_MISO=12
  -DTFT_MOSI=13
  -DTFT_SCLK=14
  -DTFT_CS=15
  -DTFT_DC=2
  -DTFT_RST=-1
  -DLOAD_GLCD=1
  -DLOAD_FONT2=1
  -DLOAD_FONT4=1
  -DLOAD_FONT6=1
  -DLOAD_GFXFF=1
  -DSMOOTH_FONT=1
  -DSPI_FREQUENCY=55000000
  -DSPI_READ_FREQUENCY=20000000
`;
}

export function applyConfigToFile(file: FirmwareFile, cfg: CYDHardwareConfig): string {
  if (file.path.endsWith('config.h')) {
    return buildCustomConfigH(cfg);
  }
  if (file.path.endsWith('User_Setup_CYD.h')) {
    return buildCustomUserSetupH(cfg);
  }
  if (file.path === 'platformio.ini') {
    return buildCustomPlatformioIni(cfg);
  }
  return file.content;
}

export async function downloadFirmwareZip(files: FirmwareFile[], cfg: CYDHardwareConfig) {
  const zip = new JSZip();
  for (const file of files) {
    const content = applyConfigToFile(file, cfg);
    zip.file(file.path, content);
  }
  const blob = await zip.generateAsync({ type: 'blob' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `DisplayConnect_Moto_CYD_${cfg.tftDriver === 'ST7789_DRIVER' ? 'ST7789' : 'ILI9341'}.zip`;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

export async function downloadAndroidPatchZip(androidFiles: FirmwareFile[]) {
  const zip = new JSZip();
  for (const file of androidFiles) {
    zip.file(file.path, file.content);
  }
  const blob = await zip.generateAsync({ type: 'blob' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = 'DisplayConnect_Android_WhatsApp_Call_Patch.zip';
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

