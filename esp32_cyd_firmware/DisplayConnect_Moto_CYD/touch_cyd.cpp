#include "touch_cyd.h"
#include "config.h"
#include "nav_types.h"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

// ESP32-2432S028R (CYD): XPT2046 uses VSPI while TFT uses HSPI
static const int RAW_X_MIN = 200;
static const int RAW_X_MAX = 3800;
static const int RAW_Y_MIN = 200;
static const int RAW_Y_MAX = 3800;

// Poll by pressure (255 disables IRQ gating, which avoids missed taps on CYD)
static SPIClass s_touchSpi(VSPI);
static XPT2046_Touchscreen s_ts(TOUCH_CS_PIN, 255);
static bool s_ok = false;

bool touchInit() {
  pinMode(TOUCH_CS_PIN, OUTPUT);
  digitalWrite(TOUCH_CS_PIN, HIGH);
  pinMode(TOUCH_IRQ_PIN, INPUT);

  s_touchSpi.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
  if (!s_ts.begin(s_touchSpi)) {
    Serial.println(F("XPT2046 Touch init failed"));
    return false;
  }

  s_ts.setRotation(TFT_ROTATION);
  s_ok = true;
  Serial.println(F("XPT2046 Touch ready (poll mode)"));
  return true;
}

bool touchRead(uint16_t& x, uint16_t& y) {
  if (!s_ok || !s_ts.touched()) {
    return false;
  }

  const TS_Point p = s_ts.getPoint();
  if (p.z < 200) {
    return false;
  }

  const int maxW = (TFT_ROTATION % 2 == 0) ? SCR_W : SCR_H;
  const int maxH = (TFT_ROTATION % 2 == 0) ? SCR_H : SCR_W;

  int16_t px = map(p.x, RAW_X_MIN, RAW_X_MAX, 0, maxW - 1);
  int16_t py = map(p.y, RAW_Y_MIN, RAW_Y_MAX, 0, maxH - 1);

  if (px < 0) px = 0;
  if (py < 0) py = 0;
  if (px >= maxW) px = maxW - 1;
  if (py >= maxH) py = maxH - 1;

  x = static_cast<uint16_t>(px);
  y = static_cast<uint16_t>(py);
  return true;
}
