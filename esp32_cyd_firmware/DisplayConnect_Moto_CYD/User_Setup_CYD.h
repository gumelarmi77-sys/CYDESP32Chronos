// =============================================================================
// TFT_eSPI User_Setup.h for ESP32-2432S028R (Cheap Yellow Display / CYD)
// Copy this to Arduino/libraries/TFT_eSPI/User_Setup.h (if using Arduino IDE)
// Note: If using PlatformIO, platformio.ini already passes these via build_flags!
// =============================================================================

#define USER_SETUP_INFO "ESP32_CYD_2432S028R"

// Use ILI9341_2_DRIVER for standard 1-USB CYD (ILI9341)
// If your CYD has 2 USB ports (USB-C + Micro-USB) with inverted/blank display,
// comment ILI9341_2_DRIVER and uncomment ST7789_DRIVER + TFT_RGB_ORDER!
#define ILI9341_2_DRIVER
// #define ST7789_DRIVER
// #define TFT_RGB_ORDER TFT_BGR

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
// #define TOUCH_CS 33

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
