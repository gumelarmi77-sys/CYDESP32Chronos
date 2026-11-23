#include "map_renderer.h"
#include "html_renderer.h"
#include "maps_theme.h"
#include "utf8_text.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.01745329251f
#endif

// Strip compositor dimensions covering dynamic map viewport y = 28..256 (6 strips x 38px)
static const int16_t MAP_TOP_Y  = 28;
static const int16_t STRIP_H    = 38;
static const int16_t STRIP_CNT  = 6; // 6 * 38 = 228 px -> covers y = 28..256

// Road & Route anti-aliased stroke widths
static const float STREET_EDGE_W = 6.2f;
static const float STREET_FILL_W = 3.2f;
static const float ROUTE_EDGE_W  = 8.4f;
static const float ROUTE_FILL_W  = 4.6f;

// Bottom-right Theme Switch Pill inside Bottom Telemetry Pill
#define SW_W 44
#define SW_H 24
#define SW_X (SCR_W - 14 - SW_W)
#define SW_Y 268

static inline float clampf(float v, float lo, float hi) {
  return (v < lo) ? lo : ((v > hi) ? hi : v);
}

static inline float easeOutCubic(float t) {
  t = clampf(t, 0.0f, 1.0f);
  const float inv = 1.0f - t;
  return 1.0f - inv * inv * inv;
}

static inline float shortestAngleDelta(float targetDeg, float currentDeg) {
  float diff = targetDeg - currentDeg;
  while (diff > 180.0f)  diff -= 360.0f;
  while (diff < -180.0f) diff += 360.0f;
  return diff;
}

static inline float normalizeDeg360(float deg) {
  while (deg >= 360.0f) deg -= 360.0f;
  while (deg < 0.0f)    deg += 360.0f;
  return deg;
}

static inline void catmullRomPoint(float p0x, float p0y, float p1x, float p1y,
                                   float p2x, float p2y, float p3x, float p3y,
                                   float t, float& outX, float& outY) {
  const float t2 = t * t;
  const float t3 = t2 * t;
  outX = 0.5f * ((2.0f * p1x) +
                 (-p0x + p2x) * t +
                 (2.0f * p0x - 5.0f * p1x + 4.0f * p2x - p3x) * t2 +
                 (-p0x + 3.0f * p1x - 3.0f * p2x + p3x) * t3);
  outY = 0.5f * ((2.0f * p1y) +
                 (-p0y + p2y) * t +
                 (2.0f * p0y - 5.0f * p1y + 4.0f * p2y - p3y) * t2 +
                 (-p0y + 3.0f * p1y - 3.0f * p2y + p3y) * t3);
}

static inline uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t alpha255) {
  const uint8_t fr = ((fg >> 11) & 0x1F) << 3;
  const uint8_t fg_g = ((fg >> 5) & 0x3F) << 2;
  const uint8_t fb = (fg & 0x1F) << 3;
  const uint8_t br = ((bg >> 11) & 0x1F) << 3;
  const uint8_t bg_g = ((bg >> 5) & 0x3F) << 2;
  const uint8_t bb = (bg & 0x1F) << 3;

  const uint8_t r = static_cast<uint8_t>((fr * alpha255 + br * (255 - alpha255)) / 255);
  const uint8_t g = static_cast<uint8_t>((fg_g * alpha255 + bg_g * (255 - alpha255)) / 255);
  const uint8_t b = static_cast<uint8_t>((fb * alpha255 + bb * (255 - alpha255)) / 255);
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static bool strContainsIgnoreCase(const char* haystack, const char* needle) {
  if (!haystack || !needle) return false;
  size_t nLen = strlen(needle);
  if (nLen == 0) return true;
  for (const char* p = haystack; *p; ++p) {
    size_t i = 0;
    while (i < nLen && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
      i++;
    }
    if (i == nLen) return true;
  }
  return false;
}

MapRenderer::MapRenderer(TFT_eSPI& display)
  : tft(display),
    chevronSpr(&display),
    digitCellSpr(&display),
    barSpr(&display),
    mapStripSpr(&display) {}

void MapRenderer::begin() {
  // Permanent Sprite Pool (22.55 KB total, allocated once -> zero heap fragmentation):
  // 1. Chevron 24x24 (1.13 KB)
  if (!chevronReady) {
    chevronSpr.setColorDepth(16);
    if (chevronSpr.createSprite(24, 24) != nullptr) {
      chevronSpr.setPivot(12, 12);
      chevronReady = true;
    }
  }
  // 2. Tabular Digit Cell 16x28 (0.88 KB)
  if (!digitCellReady) {
    digitCellSpr.setColorDepth(16);
    if (digitCellSpr.createSprite(16, 28) != nullptr) {
      digitCellReady = true;
    }
  }
  // 3. Peak-Hold Speed/RPM Bar 96x8 (1.50 KB)
  if (!barReady) {
    barSpr.setColorDepth(16);
    if (barSpr.createSprite(96, 8) != nullptr) {
      barReady = true;
    }
  }
  // 4. Strip Compositor 240x38 (17.81 KB)
  if (!mapStripReady) {
    mapStripSpr.setColorDepth(16);
    if (mapStripSpr.createSprite(SCR_W, STRIP_H) != nullptr) {
      mapStripReady = true;
    }
  }
  buildChevronSprite();
}

void MapRenderer::refreshThemeAssets() {
  buildChevronSprite();
  invalidateLayout();
}

void MapRenderer::invalidateLayout() {
  topBarDrawn = false;
  bottomPillFrameDrawn = false;
  hudCardsDrawn = false;
  hudStaticDirty = true;
  mapDirty = true;
  memset(lastSpeedChars, ' ', 3);
  lastSpeedChars[3] = '\0';
  lastEtaText[0] = '\0';
  lastMetaText[0] = '\0';
  lastHudDist[0] = '\0';
  lastBarFillPx = -1;
  lastBarPeakPx = -1;
}

void MapRenderer::buildChevronSprite() {
  if (!chevronReady) return;
  chevronSpr.fillSprite(TFT_TRANSPARENT);

  const uint16_t shadowCol = 0x0841; // Soft near-black shadow
  const uint16_t whiteCol  = mapsColWhite();
  const uint16_t mainCol   = mapsColAccent();
  const uint16_t shadeCol  = blend565(mainCol, shadowCol, 185);

  // Drop shadow offset +2px Y
  chevronSpr.fillTriangle(12, 4, 4, 21, 12, 17, shadowCol);
  chevronSpr.fillTriangle(12, 4, 20, 21, 12, 17, shadowCol);

  // Smooth anti-aliased white halo border
  chevronSpr.drawWideLine(12, 3, 4, 19, 3.0f, whiteCol, TFT_TRANSPARENT);
  chevronSpr.drawWideLine(12, 3, 20, 19, 3.0f, whiteCol, TFT_TRANSPARENT);
  chevronSpr.fillTriangle(12, 2, 3, 20, 12, 15, whiteCol);
  chevronSpr.fillTriangle(12, 2, 21, 20, 12, 15, whiteCol);

  // 3D beveled left & right wings
  chevronSpr.fillTriangle(12, 4, 5, 19, 12, 14, mainCol);
  chevronSpr.fillTriangle(12, 4, 19, 19, 12, 14, shadeCol);
  chevronSpr.drawLine(12, 4, 12, 14, whiteCol);
}

void MapRenderer::setTargetNav(const NavState& state) {
  // Detect maneuver / street change to trigger cubic ease-out card transition
  char newSig[96];
  snprintf(newSig, sizeof(newSig), "%s|%s|%d",
           state.instruction, state.street, state.offRoute ? 1 : 0);
  if (!hasTarget || strcmp(newSig, prevManeuverSig) != 0) {
    strncpy(prevManeuverSig, newSig, sizeof(prevManeuverSig) - 1);
    prevManeuverSig[sizeof(prevManeuverSig) - 1] = '\0';
    turnAnimT = 0.0f;
    hudStaticDirty = true;
  }

  targetNav = state;

  if (!hasTarget) {
    hasTarget = true;
    curLat = state.lat;
    curLon = state.lon;
    curBearing = normalizeDeg360(state.bearing);
    curSpeedKmh = (state.speedKmh >= 0.0f) ? state.speedKmh : 0.0f;
    peakSpeedKmh = curSpeedKmh;
    curDistanceM = static_cast<float>(state.distanceM);
    curUserX = state.hasUserPosition ? static_cast<float>(state.userX) : 120.0f;
    curUserY = state.hasUserPosition ? static_cast<float>(state.userY) : 170.0f;

    curRouteCount = state.routeCount;
    for (int i = 0; i < curRouteCount; i++) {
      curRouteX[i] = static_cast<float>(state.routeX[i]);
      curRouteY[i] = static_cast<float>(state.routeY[i]);
    }
    curStreetCount = state.streetSegmentCount;
    for (int i = 0; i < curStreetCount; i++) {
      curStreetX0[i] = static_cast<float>(state.streetX0[i]);
      curStreetY0[i] = static_cast<float>(state.streetY0[i]);
      curStreetX1[i] = static_cast<float>(state.streetX1[i]);
      curStreetY1[i] = static_cast<float>(state.streetY1[i]);
    }
  } else {
    // Snap geometry arrays only if vertex/segment topology count changed
    if (state.routeCount != curRouteCount) {
      curRouteCount = state.routeCount;
      for (int i = 0; i < curRouteCount; i++) {
        curRouteX[i] = static_cast<float>(state.routeX[i]);
        curRouteY[i] = static_cast<float>(state.routeY[i]);
      }
    }
    if (state.streetSegmentCount != curStreetCount) {
      curStreetCount = state.streetSegmentCount;
      for (int i = 0; i < curStreetCount; i++) {
        curStreetX0[i] = static_cast<float>(state.streetX0[i]);
        curStreetY0[i] = static_cast<float>(state.streetY0[i]);
        curStreetX1[i] = static_cast<float>(state.streetX1[i]);
        curStreetY1[i] = static_cast<float>(state.streetY1[i]);
      }
    }
  }

  mapDirty = true;
}

void MapRenderer::triggerNotifyToast(const NotifyState& notify) {
  toastState = notify;
  if (!notify.active || notify.kind == NOTIFY_NONE) {
    dismissNotifyToast();
    return;
  }
  toastActive = true;
  toastClosing = false;
  toastAnimT = 0.0f;
  hudStaticDirty = true;
  mapDirty = true;
}

void MapRenderer::dismissNotifyToast() {
  if (!toastActive) return;
  toastClosing = true;
  hudStaticDirty = true;
}

void MapRenderer::tick(float dt, uint32_t nowMs, const HwGpsData& hwGps, bool bleConnected, bool hasNav) {
  (void)nowMs;
  dt = clampf(dt, 0.001f, 0.05f);

  const DisplayViewMode mode = motoViewModeGet();
  if (mode == VIEW_MEDIA_PLAYER || !hasNav || !hasTarget) {
    return;
  }

  // 1. Frame-rate independent physics smoothing
  const float coordAlpha = 1.0f - expf(-7.5f * dt);
  const float angleAlpha = 1.0f - expf(-9.0f * dt);
  const float speedAlpha = 1.0f - expf(-8.0f * dt);

  bool moved = false;

  // Shortest-angle heading interpolation (0..360 wrap)
  const float targetBrg = normalizeDeg360(targetNav.bearing);
  const float angDiff = shortestAngleDelta(targetBrg, curBearing);
  if (fabsf(angDiff) > 0.15f) {
    curBearing = normalizeDeg360(curBearing + angDiff * angleAlpha);
    moved = true;
  } else {
    curBearing = targetBrg;
  }

  // Lat/Lon smoothing
  curLat += (targetNav.lat - curLat) * coordAlpha;
  curLon += (targetNav.lon - curLon) * coordAlpha;

  // Speedometer smoothing + Peak-Hold with natural gravity decay
  const float targetSpd = (targetNav.speedKmh >= 0.0f)
    ? targetNav.speedKmh
    : (hwGps.fixValid ? hwGps.speedKmh : 0.0f);
  if (fabsf(targetSpd - curSpeedKmh) > 0.08f) {
    curSpeedKmh += (targetSpd - curSpeedKmh) * speedAlpha;
  } else {
    curSpeedKmh = targetSpd;
  }

  if (curSpeedKmh >= peakSpeedKmh) {
    peakSpeedKmh = curSpeedKmh;
    peakHoldTimer = 0.45f; // Hold peak briefly before falling
    peakFallVel = 0.0f;
  } else if (peakHoldTimer > 0.0f) {
    peakHoldTimer -= dt;
  } else {
    peakFallVel += 95.0f * dt; // Gravity acceleration (km/h / s^2)
    peakSpeedKmh -= peakFallVel * dt;
    if (peakSpeedKmh < curSpeedKmh) {
      peakSpeedKmh = curSpeedKmh;
      peakFallVel = 0.0f;
    }
  }

  // Distance to maneuver smoothing
  const float targetDist = static_cast<float>(targetNav.distanceM);
  if (fabsf(targetDist - curDistanceM) > 0.2f) {
    curDistanceM += (targetDist - curDistanceM) * speedAlpha;
  } else {
    curDistanceM = targetDist;
  }

  // User position & vector route/street coordinate interpolation
  if (targetNav.hasUserPosition) {
    const float ux = static_cast<float>(targetNav.userX);
    const float uy = static_cast<float>(targetNav.userY);
    if (fabsf(ux - curUserX) > 0.12f || fabsf(uy - curUserY) > 0.12f) {
      curUserX += (ux - curUserX) * coordAlpha;
      curUserY += (uy - curUserY) * coordAlpha;
      moved = true;
    } else {
      curUserX = ux;
      curUserY = uy;
    }
  }

  for (int i = 0; i < curRouteCount; i++) {
    const float tx = static_cast<float>(targetNav.routeX[i]);
    const float ty = static_cast<float>(targetNav.routeY[i]);
    if (fabsf(tx - curRouteX[i]) > 0.12f || fabsf(ty - curRouteY[i]) > 0.12f) {
      curRouteX[i] += (tx - curRouteX[i]) * coordAlpha;
      curRouteY[i] += (ty - curRouteY[i]) * coordAlpha;
      moved = true;
    } else {
      curRouteX[i] = tx;
      curRouteY[i] = ty;
    }
  }

  for (int i = 0; i < curStreetCount; i++) {
    const float tx0 = static_cast<float>(targetNav.streetX0[i]);
    const float ty0 = static_cast<float>(targetNav.streetY0[i]);
    const float tx1 = static_cast<float>(targetNav.streetX1[i]);
    const float ty1 = static_cast<float>(targetNav.streetY1[i]);
    if (fabsf(tx0 - curStreetX0[i]) > 0.15f || fabsf(ty0 - curStreetY0[i]) > 0.15f ||
        fabsf(tx1 - curStreetX1[i]) > 0.15f || fabsf(ty1 - curStreetY1[i]) > 0.15f) {
      curStreetX0[i] += (tx0 - curStreetX0[i]) * coordAlpha;
      curStreetY0[i] += (ty0 - curStreetY0[i]) * coordAlpha;
      curStreetX1[i] += (tx1 - curStreetX1[i]) * coordAlpha;
      curStreetY1[i] += (ty1 - curStreetY1[i]) * coordAlpha;
      moved = true;
    }
  }

  // 2. Ease-out UI card & toast transitions
  if (turnAnimT < 1.0f) {
    turnAnimT = clampf(turnAnimT + dt / 0.30f, 0.0f, 1.0f);
    moved = true;
  }

  if (toastActive) {
    if (!toastClosing && toastAnimT < 1.0f) {
      toastAnimT = clampf(toastAnimT + dt / 0.28f, 0.0f, 1.0f);
      moved = true;
    } else if (toastClosing) {
      toastAnimT = clampf(toastAnimT - dt / 0.22f, 0.0f, 1.0f);
      moved = true;
      if (toastAnimT <= 0.001f) {
        toastActive = false;
        toastClosing = false;
        toastState.active = false;
        hudStaticDirty = true;
      }
    }
  }

  if (moved) {
    mapDirty = true;
  }

  // 3. Render only dirty regions
  if (!topBarDrawn || bleConnected != lastBleStatus || hwGps.satellites != lastSatCount) {
    lastBleStatus = bleConnected;
    lastSatCount = hwGps.satellites;
    drawTopNavButtons(bleConnected, &hwGps);
    topBarDrawn = true;
  }

  if (mode == VIEW_MAP_VECTOR) {
    if (mapDirty) {
      renderMapViewportBands();
      mapDirty = false;
    }
    drawBottomTelemetryFrame(false);
    updateBottomTelemetryDynamic(hwGps, bleConnected, false);
  } else if (mode == VIEW_MOTO_HUD) {
    if (!hudCardsDrawn || hudStaticDirty) {
      drawMotoHudMode(targetNav, hwGps, bleConnected);
      hudCardsDrawn = true;
      hudStaticDirty = false;
      mapDirty = false;
    } else {
      updateMotoHudDynamic(hwGps, bleConnected);
      drawBottomTelemetryFrame(false);
      updateBottomTelemetryDynamic(hwGps, bleConnected, false);
      mapDirty = false;
    }
  }
}

void MapRenderer::drawFloatingCard(TFT_eSPI& dst, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                                   uint16_t fill, uint16_t border, uint16_t bg) {
  dst.fillSmoothRoundRect(x + 2, y + 3, w, h, r, mapsColCardShadow(), bg);
  dst.fillSmoothRoundRect(x, y, w, h, r, fill, bg);
  dst.drawSmoothRoundRect(x, y, r, (r > 1 ? r - 1 : 0), w, h, border, fill);
}

void MapRenderer::drawMapBackdropInto(TFT_eSPI& dst, int16_t offsetY) {
  const uint16_t land = mapsColLand();
  const uint16_t park = mapsColPark();
  const uint16_t grid = mapsColGrid();

  // Subtle green/park urban blocks
  dst.fillSmoothRoundRect(12,  96 - offsetY,  56, 48, 8, park, land);
  dst.fillSmoothRoundRect(172, 134 - offsetY, 58, 62, 8, park, land);
  dst.fillSmoothRoundRect(16,  194 - offsetY, 48, 44, 8, park, land);

  for (int16_t x = 40; x < SCR_W; x += 40) {
    dst.drawFastVLine(x, 0, STRIP_H, grid);
  }
  for (int16_t gy = 40; gy < SCR_H; gy += 40) {
    const int16_t localY = gy - offsetY;
    if (localY >= 0 && localY < STRIP_H) {
      dst.drawFastHLine(0, localY, SCR_W, grid);
    }
  }
}

void MapRenderer::clearMapArea() {
  tft.fillScreen(mapsColLand());
  const uint16_t land = mapsColLand();
  const uint16_t park = mapsColPark();
  const uint16_t grid = mapsColGrid();
  tft.fillSmoothRoundRect(12, 96, 56, 48, 8, park, land);
  tft.fillSmoothRoundRect(172, 134, 58, 62, 8, park, land);
  tft.fillSmoothRoundRect(16, 194, 48, 44, 8, park, land);
  for (int16_t x = 40; x < SCR_W; x += 40) tft.drawFastVLine(x, 0, SCR_H, grid);
  for (int16_t y = 40; y < SCR_H; y += 40) tft.drawFastHLine(0, y, SCR_W, grid);
}

void MapRenderer::drawThemeSwitchAt(int sx, int sy, uint16_t bg) {
  const bool isDark = mapsThemeIsDark();
  const uint16_t trackCol = mapsColSwitchTrack();
  const uint16_t knobCol  = mapsColSwitchKnob();

  tft.fillSmoothRoundRect(sx, sy, SW_W, SW_H, 12, trackCol, bg);
  tft.drawSmoothRoundRect(sx, sy, 12, 11, SW_W, SW_H, mapsColCardBorder(), trackCol);

  const int16_t knobCx = isDark ? (sx + SW_W - 11) : (sx + 11);
  const int16_t knobCy = sy + SW_H / 2;
  tft.drawSpot(knobCx, knobCy, 7.0f, knobCol, trackCol);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(mapsColText(), trackCol);
  tft.drawString(isDark ? "NGT" : "DAY", isDark ? (sx + 14) : (sx + SW_W - 14), knobCy + 1, 1);
  tft.setTextDatum(TL_DATUM);
}

void MapRenderer::drawHudButtonTopRight(uint16_t bg) {
  const DisplayViewMode mode = motoViewModeGet();
  const char* label = (mode == VIEW_MAP_VECTOR)
    ? "HUD"
    : (mode == VIEW_MOTO_HUD ? "MUS" : "MAP");

  const uint16_t card = mapsColCard();
  const uint16_t accent = mapsColMonoEye();
  tft.fillSmoothRoundRect(SCR_W - 52, 5, 44, 20, 10, card, bg);
  tft.drawSmoothRoundRect(SCR_W - 52, 5, 10, 9, 44, 20, accent, card);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(accent, card);
  tft.drawString(label, SCR_W - 30, 15, 1);
  tft.setTextDatum(TL_DATUM);
}

void MapRenderer::drawBriButtonTopLeft(uint16_t bg) {
  const uint8_t lvl = backlightLevelGet();
  const char* label = (lvl == 0) ? "25%" : (lvl == 1 ? "65%" : "100%");
  const uint16_t card = mapsColCard();

  tft.fillSmoothRoundRect(8, 5, 44, 20, 10, card, bg);
  tft.drawSmoothRoundRect(8, 5, 10, 9, 44, 20, mapsColCardBorder(), card);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(mapsColText(), card);
  tft.drawString(label, 30, 15, 1);
  tft.setTextDatum(TL_DATUM);
}

void MapRenderer::drawTopStatusPill(bool bleConnected, const HwGpsData* hwGps, uint16_t bg) {
  const uint16_t card = mapsColCard();
  const int16_t px = 60;
  const int16_t pw = 120;
  tft.fillSmoothRoundRect(px, 5, pw, 20, 10, card, bg);
  tft.drawSmoothRoundRect(px, 5, 10, 9, pw, 20, mapsColCardBorder(), card);

  const uint16_t bleDot = bleConnected ? mapsColSuccess() : mapsColDanger();
  tft.drawSpot(px + 12, 15, 3.0f, bleDot, card);

  char statusBuf[24];
  if (hwGps != nullptr && hwGps->fixValid) {
    snprintf(statusBuf, sizeof(statusBuf), "BLE  SAT:%d", hwGps->satellites);
  } else if (bleConnected) {
    snprintf(statusBuf, sizeof(statusBuf), mapsThemeAutoGet() ? "BLE  AUTO" : "BLE  LINK");
  } else {
    snprintf(statusBuf, sizeof(statusBuf), "NO BLE");
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(mapsColMuted(), card);
  tft.drawString(statusBuf, px + (pw / 2) + 5, 15, 1);
  tft.setTextDatum(TL_DATUM);
}

void MapRenderer::drawTopNavButtons(bool bleConnected, const HwGpsData* hwGps) {
  const uint16_t bg = mapsColLand();
  tft.fillRect(0, 0, SCR_W, MAP_TOP_Y, bg);
  drawBriButtonTopLeft(bg);
  drawTopStatusPill(bleConnected, hwGps, bg);
  drawHudButtonTopRight(bg);
}

void MapRenderer::drawThemeSwitch() {
  drawThemeSwitchAt(SW_X, SW_Y, mapsColCard());
}

void MapRenderer::drawThemeSwitchWaiting() {
  drawThemeSwitchAt(SCR_W - 14 - SW_W, SCR_H - 42, mapsColLand());
}

bool MapRenderer::themeSwitchHit(uint16_t x, uint16_t y) {
  return (y >= 240 && x >= static_cast<uint16_t>(SCR_W - 86));
}

bool MapRenderer::viewModeSwitchHit(uint16_t x, uint16_t y) {
  return (x >= SCR_W - 68 && y <= 40);
}

bool MapRenderer::brightnessSwitchHit(uint16_t x, uint16_t y) {
  return (x <= 68 && y <= 40);
}

bool MapRenderer::notifyDismissHit(uint16_t x, uint16_t y) {
  (void)x;
  // Covers sliding bottom toast area (y = 190..252) without overlapping bottom pill
  return (y >= 190 && y <= 252);
}

MediaCommand MapRenderer::mediaControlHit(uint16_t x, uint16_t y) {
  if (y >= 118 && y <= 178) {
    if (x >= 4 && x <= 80) return MEDIA_CMD_PREV;
    if (x >= 82 && x <= 158) return MEDIA_CMD_PLAY_PAUSE;
    if (x >= 160 && x <= 236) return MEDIA_CMD_NEXT;
  }
  if (y >= 184 && y <= 240) {
    if (x >= 4 && x <= 80) return MEDIA_CMD_VOL_DOWN;
    if (x >= 82 && x <= 158) return MEDIA_CMD_STOP;
    if (x >= 160 && x <= 236) return MEDIA_CMD_VOL_UP;
  }
  return MEDIA_CMD_NONE;
}

void MapRenderer::drawToastInto(TFT_eSPI& dst, int16_t toastY, int16_t offsetY) {
  if (!toastActive || toastState.kind == NOTIFY_NONE) return;

  const int16_t bx = 8;
  const int16_t by = toastY - offsetY;
  const int16_t bw = SCR_W - 16;
  const int16_t bh = 56;
  if (by + bh < -4 || by > STRIP_H + 4) return;

  const uint16_t card = mapsColCard();
  const uint16_t accentCol = (toastState.kind == NOTIFY_CALL)
    ? mapsColDanger()
    : (toastState.kind == NOTIFY_WHATSAPP
        ? mapsColSuccess()
        : (toastState.kind == NOTIFY_MEDIA ? mapsColAccent() : mapsColMonoEye()));

  drawFloatingCard(dst, bx, by, bw, bh, 12, card, accentCol, mapsColLand());
  dst.fillSmoothRoundRect(bx + 6, by + 8, 4, bh - 16, 2, accentCol, card);

  const char* tag = (toastState.kind == NOTIFY_CALL)
    ? "PANGGILAN MASUK"
    : (toastState.kind == NOTIFY_WHATSAPP
        ? "WHATSAPP"
        : (toastState.kind == NOTIFY_MEDIA ? "NOW PLAYING" : "NOTIFIKASI"));

  dst.setTextDatum(TL_DATUM);
  dst.setTextColor(accentCol, card);
  dst.drawString(tag, bx + 16, by + 6, 1);

  dst.setTextDatum(TR_DATUM);
  dst.setTextColor(mapsColMuted(), card);
  dst.drawString("TAP TUTUP", bx + bw - 10, by + 6, 1);
  dst.setTextDatum(TL_DATUM);

  if (toastState.sender[0] != '\0' && (by + 16 < STRIP_H && by + 36 > 0)) {
    drawUtf8Text(dst, toastState.sender, bx + 16, by + 16, bw - 26, mapsColText(), card);
  }
  if (toastState.message[0] != '\0' && (by + 35 < STRIP_H && by + 55 > 0)) {
    drawUtf8Text(dst, toastState.message, bx + 16, by + 35, bw - 26, mapsColMuted(), card);
  }
}

void MapRenderer::drawNotifyBanner(const NotifyState& notify) {
  triggerNotifyToast(notify);
  if (motoViewModeGet() == VIEW_MAP_VECTOR && mapStripReady && hasTarget) {
    renderMapViewportBands();
  } else if (toastActive) {
    drawToastInto(tft, 196, 0);
  }
}

void MapRenderer::drawMiniManeuverArrow(TFT_eSPI& dst, int16_t cx, int16_t cy,
                                        const char* instruction, uint16_t col, uint16_t bg) {
  if (strContainsIgnoreCase(instruction, "kanan") || strContainsIgnoreCase(instruction, "right") || strContainsIgnoreCase(instruction, "direita")) {
    dst.drawWideLine(cx - 7, cy + 12, cx - 7, cy - 3, 4.5f, col, bg);
    dst.drawWideLine(cx - 7, cy - 3, cx + 9, cy - 3, 4.5f, col, bg);
    dst.drawSpot(cx - 7, cy - 3, 2.2f, col, bg);
    dst.drawWideLine(cx + 3, cy - 10, cx + 11, cy - 3, 3.6f, col, bg);
    dst.drawWideLine(cx + 3, cy + 4, cx + 11, cy - 3, 3.6f, col, bg);
    dst.drawSpot(cx + 11, cy - 3, 1.8f, col, bg);
  } else if (strContainsIgnoreCase(instruction, "kiri") || strContainsIgnoreCase(instruction, "left") || strContainsIgnoreCase(instruction, "esquerda")) {
    dst.drawWideLine(cx + 7, cy + 12, cx + 7, cy - 3, 4.5f, col, bg);
    dst.drawWideLine(cx + 7, cy - 3, cx - 9, cy - 3, 4.5f, col, bg);
    dst.drawSpot(cx + 7, cy - 3, 2.2f, col, bg);
    dst.drawWideLine(cx - 3, cy - 10, cx - 11, cy - 3, 3.6f, col, bg);
    dst.drawWideLine(cx - 3, cy + 4, cx - 11, cy - 3, 3.6f, col, bg);
    dst.drawSpot(cx - 11, cy - 3, 1.8f, col, bg);
  } else if (strContainsIgnoreCase(instruction, "putar") || strContainsIgnoreCase(instruction, "u-turn") || strContainsIgnoreCase(instruction, "retorno")) {
    dst.drawSmoothArc(cx, cy - 3, 10, 6, 90, 270, col, bg, true);
    dst.drawWideLine(cx + 8, cy - 3, cx + 8, cy + 12, 4.0f, col, bg);
    dst.drawWideLine(cx - 8, cy - 3, cx - 8, cy + 8, 4.0f, col, bg);
    dst.drawWideLine(cx - 13, cy + 3, cx - 8, cy + 10, 3.2f, col, bg);
    dst.drawWideLine(cx - 3, cy + 3, cx - 8, cy + 10, 3.2f, col, bg);
  } else if (strContainsIgnoreCase(instruction, "tiba") || strContainsIgnoreCase(instruction, "arrive") || strContainsIgnoreCase(instruction, "chegou")) {
    dst.drawSmoothArc(cx, cy, 12, 9, 0, 360, col, bg, false);
    dst.drawSpot(cx, cy, 5.0f, mapsColMonoEye(), bg);
    dst.drawSpot(cx, cy, 2.0f, mapsColWhite(), mapsColMonoEye());
  } else {
    dst.drawWideLine(cx, cy + 13, cx, cy - 9, 4.5f, col, bg);
    dst.drawWideLine(cx - 8, cy - 2, cx, cy - 11, 3.8f, col, bg);
    dst.drawWideLine(cx + 8, cy - 2, cx, cy - 11, 3.8f, col, bg);
    dst.drawSpot(cx, cy - 11, 2.0f, col, bg);
  }
}

void MapRenderer::drawBigManeuverArrow(int16_t cx, int16_t cy, const char* instruction, uint16_t bg) {
  const uint16_t stemCol = mapsColMonoEye();
  const uint16_t tipCol  = mapsColAccent();

  tft.drawSmoothArc(cx, cy, 46, 44, 0, 360, mapsColCardBorder(), bg, false);

  if (strContainsIgnoreCase(instruction, "kanan") || strContainsIgnoreCase(instruction, "right") || strContainsIgnoreCase(instruction, "direita")) {
    tft.drawWideLine(cx - 18, cy + 28, cx - 18, cy - 6, 10.0f, stemCol, bg);
    tft.drawWideLine(cx - 18, cy - 6, cx + 18, cy - 6, 10.0f, stemCol, bg);
    tft.drawSpot(cx - 18, cy - 6, 5.0f, stemCol, bg);
    tft.drawWideLine(cx + 4, cy - 22, cx + 24, cy - 6, 8.5f, tipCol, bg);
    tft.drawWideLine(cx + 4, cy + 10, cx + 24, cy - 6, 8.5f, tipCol, bg);
    tft.drawSpot(cx + 24, cy - 6, 4.2f, tipCol, bg);
  } else if (strContainsIgnoreCase(instruction, "kiri") || strContainsIgnoreCase(instruction, "left") || strContainsIgnoreCase(instruction, "esquerda")) {
    tft.drawWideLine(cx + 18, cy + 28, cx + 18, cy - 6, 10.0f, stemCol, bg);
    tft.drawWideLine(cx + 18, cy - 6, cx - 18, cy - 6, 10.0f, stemCol, bg);
    tft.drawSpot(cx + 18, cy - 6, 5.0f, stemCol, bg);
    tft.drawWideLine(cx - 4, cy - 22, cx - 24, cy - 6, 8.5f, tipCol, bg);
    tft.drawWideLine(cx - 4, cy + 10, cx - 24, cy - 6, 8.5f, tipCol, bg);
    tft.drawSpot(cx - 24, cy - 6, 4.2f, tipCol, bg);
  } else if (strContainsIgnoreCase(instruction, "putar") || strContainsIgnoreCase(instruction, "u-turn") || strContainsIgnoreCase(instruction, "retorno")) {
    tft.drawSmoothArc(cx, cy - 8, 22, 13, 90, 270, stemCol, bg, true);
    tft.drawWideLine(cx + 17, cy - 8, cx + 17, cy + 26, 9.0f, stemCol, bg);
    tft.drawWideLine(cx - 17, cy - 8, cx - 17, cy + 16, 9.0f, stemCol, bg);
    tft.drawWideLine(cx - 28, cy + 4, cx - 17, cy + 20, 7.5f, tipCol, bg);
    tft.drawWideLine(cx - 6, cy + 4, cx - 17, cy + 20, 7.5f, tipCol, bg);
    tft.drawSpot(cx - 17, cy + 20, 3.8f, tipCol, bg);
  } else if (strContainsIgnoreCase(instruction, "tiba") || strContainsIgnoreCase(instruction, "arrive") || strContainsIgnoreCase(instruction, "chegou")) {
    tft.drawSmoothArc(cx, cy, 26, 20, 0, 360, tipCol, bg, false);
    tft.drawSpot(cx, cy, 11.0f, stemCol, bg);
    tft.drawSpot(cx, cy, 4.5f, mapsColWhite(), stemCol);
  } else {
    tft.drawWideLine(cx, cy + 30, cx, cy - 18, 10.0f, stemCol, bg);
    tft.drawWideLine(cx - 18, cy - 2, cx, cy - 24, 8.5f, tipCol, bg);
    tft.drawWideLine(cx + 18, cy - 2, cx, cy - 24, 8.5f, tipCol, bg);
    tft.drawSpot(cx, cy - 24, 4.2f, tipCol, bg);
  }
}

void MapRenderer::drawFloatingTurnCardInto(TFT_eSPI& dst, int16_t cardY, int16_t offsetY) {
  const int16_t cx = 8;
  const int16_t cy = cardY - offsetY;
  const int16_t cw = SCR_W - 16;
  const int16_t ch = 56;
  if (cy + ch < -4 || cy > STRIP_H + 4) return;

  const uint16_t card = mapsColCard();
  const uint16_t border = targetNav.offRoute ? mapsColDanger() : mapsColCardBorder();

  drawFloatingCard(dst, cx, cy, cw, ch, 12, card, border, mapsColLand());

  const uint16_t iconBoxBg = mapsColCardAlt();
  const uint16_t iconCol = targetNav.offRoute ? mapsColDanger() : mapsColAccent();
  dst.fillSmoothRoundRect(cx + 6, cy + 6, 44, 44, 10, iconBoxBg, card);
  if (targetNav.hasChronosIcon) {
    // Render 48x48 1-bit Chronos maneuver bitmap (center 40x40 inside 44x44 box)
    const int16_t dstX0 = cx + 8;
    const int16_t dstY0 = cy + 8;
    for (int iy = 0; iy < 40; iy++) {
      const int16_t py = dstY0 + iy;
      if (py < 0 || py >= STRIP_H) continue;
      const int srcY = iy + 4;
      for (int ix = 0; ix < 40; ix++) {
        const int srcX = ix + 4;
        const int byteIdx = (srcY * 48 + srcX) / 8;
        const int bitPos = 7 - (srcX % 8);
        if ((targetNav.chronosIcon[byteIdx] >> bitPos) & 0x01) {
          dst.drawPixel(dstX0 + ix, py, iconCol);
        }
      }
    }
  } else {
    drawMiniManeuverArrow(dst, cx + 28, cy + 28, targetNav.instruction, iconCol, iconBoxBg);
  }

  if (targetNav.hasHtml && targetNav.html[0] != '\0') {
    HtmlRenderer htmlRenderer(dst);
    dst.setTextColor(mapsColText(), card);
    htmlRenderer.drawLines(targetNav.html, cx + 58, cy + 8, 17, 2);
    return;
  }

  const int distInt = static_cast<int>(roundf(curDistanceM));
  char distLine[28];
  if (targetNav.offRoute) {
    snprintf(distLine, sizeof(distLine), "OFF ROUTE");
  } else if (targetNav.gpsWeak) {
    const char* weakTxt = (targetNav.lang == LANG_ID) ? "GPS LEMAH" : (targetNav.lang == LANG_EN ? "WEAK GPS" : "GPS FRACO");
    snprintf(distLine, sizeof(distLine), "%s", weakTxt);
  } else if (distInt >= 1000) {
    snprintf(distLine, sizeof(distLine), "%.1f km", distInt / 1000.0f);
  } else {
    snprintf(distLine, sizeof(distLine), "%d m", distInt);
  }

  dst.setTextDatum(TL_DATUM);
  dst.setTextColor(targetNav.offRoute ? mapsColDanger() : mapsColText(), card);
  dst.drawString(distLine, cx + 58, cy + 6, 4);

  const char* subText = (targetNav.street[0] != '\0') ? targetNav.street : targetNav.instruction;
  if (subText != nullptr && subText[0] != '\0' && (cy + 33 < STRIP_H && cy + 53 > 0)) {
    drawUtf8Text(dst, subText, cx + 58, cy + 33, cw - 66, mapsColMuted(), card);
  }
}

void MapRenderer::drawTabularSpeedDigits(int speedInt, uint16_t fg, uint16_t bg, bool force) {
  if (speedInt < 0) speedInt = 0;
  if (speedInt > 999) speedInt = 999;

  char buf[4];
  snprintf(buf, sizeof(buf), "%3d", speedInt);

  // 3 uniform tabular cells (16px wide each) at x = 16, 32, 48; y = 263
  for (int i = 0; i < 3; i++) {
    if (!force && buf[i] == lastSpeedChars[i]) continue;
    lastSpeedChars[i] = buf[i];

    const int16_t cellX = 16 + i * 16;
    const int16_t cellY = 263;

    if (digitCellReady) {
      digitCellSpr.fillSprite(bg);
      if (buf[i] != ' ') {
        char ch[2] = { buf[i], '\0' };
        digitCellSpr.setTextDatum(MC_DATUM);
        digitCellSpr.setTextColor(fg, bg);
        digitCellSpr.drawString(ch, 8, 14, 4);
        digitCellSpr.setTextDatum(TL_DATUM);
      }
      digitCellSpr.pushSprite(cellX, cellY);
    } else {
      tft.fillRect(cellX, cellY, 16, 28, bg);
      if (buf[i] != ' ') {
        char ch[2] = { buf[i], '\0' };
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(fg, bg);
        tft.drawString(ch, cellX + 8, cellY + 14, 4);
        tft.setTextDatum(TL_DATUM);
      }
    }
  }
}

void MapRenderer::drawPeakHoldSpeedBar(float spd, float peak, uint16_t bg, bool force) {
  const int16_t bx = 76;
  const int16_t by = 299;
  const int16_t bw = 96; // Fits neatly between speed divider and theme switch
  const int16_t bh = 8;

  const float maxSpd = 120.0f;
  const int16_t fillPx = static_cast<int16_t>(roundf(clampf(spd / maxSpd, 0.0f, 1.0f) * bw));
  const int16_t peakPx = static_cast<int16_t>(roundf(clampf(peak / maxSpd, 0.0f, 1.0f) * (bw - 3)));

  if (!force && fillPx == lastBarFillPx && peakPx == lastBarPeakPx) {
    return;
  }
  lastBarFillPx = fillPx;
  lastBarPeakPx = peakPx;

  const uint16_t trackCol = mapsColCardAlt();
  const uint16_t fillCol  = (spd > 90.0f) ? mapsColDanger() : ((spd > 65.0f) ? mapsColAccent() : mapsColMonoEye());
  const uint16_t peakCol  = mapsColWhite();

  if (barReady) {
    barSpr.fillSprite(bg);
    barSpr.fillSmoothRoundRect(0, 1, bw, bh - 2, 3, trackCol, bg);
    if (fillPx >= 4) {
      barSpr.fillSmoothRoundRect(0, 1, fillPx, bh - 2, 3, fillCol, trackCol);
    }
    if (peakPx > 2) {
      barSpr.fillRect(peakPx, 0, 3, bh, peakCol);
    }
    barSpr.pushSprite(bx, by);
  } else {
    tft.fillSmoothRoundRect(bx, by + 1, bw, bh - 2, 3, trackCol, bg);
    if (fillPx >= 4) {
      tft.fillSmoothRoundRect(bx, by + 1, fillPx, bh - 2, 3, fillCol, trackCol);
    }
    if (peakPx > 2) {
      tft.fillRect(bx + peakPx, by, 3, bh, peakCol);
    }
  }
}

void MapRenderer::drawBottomTelemetryFrame(bool force) {
  if (bottomPillFrameDrawn && !force) return;
  bottomPillFrameDrawn = true;

  const int16_t px = 8;
  const int16_t py = 258;
  const int16_t pw = SCR_W - 16;
  const int16_t ph = 54;
  const uint16_t card = mapsColCard();

  tft.fillRect(0, 256, SCR_W, SCR_H - 256, mapsColLand());
  drawFloatingCard(tft, px, py, pw, ph, 14, card, mapsColCardBorder(), mapsColLand());

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(mapsColMuted(), card);
  tft.drawString("km/h", px + 14, py + 36, 1);

  tft.drawFastVLine(px + 60, py + 8, ph - 16, mapsColCardBorder());
  drawThemeSwitchAt(SW_X, SW_Y, card);
}

void MapRenderer::updateBottomTelemetryDynamic(const HwGpsData& hwGps, bool bleConnected, bool force) {
  const uint16_t card = mapsColCard();
  const int16_t px = 8;
  const int16_t py = 258;

  // 1. Tabular Speed Digits (dirty-checked per digit cell)
  const int dispSpd = static_cast<int>(roundf(curSpeedKmh));
  drawTabularSpeedDigits(dispSpd, mapsColAccent(), card, force);

  // 2. Peak-Hold Speed/RPM Bar (with gravity decay)
  drawPeakHoldSpeedBar(curSpeedKmh, peakSpeedKmh, card, force);

  // 3. ETA & Distance text (dirty-checked)
  char etaBuf[28] = "-- min";
  if (targetNav.offRoute) {
    snprintf(etaBuf, sizeof(etaBuf), "HITUNG ULANG");
  } else if (targetNav.valid && (targetNav.remainingM >= 0 || targetNav.remainingS >= 0)) {
    char km[12] = "--";
    char mn[12] = "--";
    if (targetNav.remainingM >= 0) {
      snprintf(km, sizeof(km), targetNav.remainingM < 1000 ? "%.2fkm" : "%.1fkm", targetNav.remainingM / 1000.0);
    }
    if (targetNav.remainingS >= 0) {
      const int mins = targetNav.remainingS / 60 + (targetNav.remainingS % 60 != 0);
      if (mins >= 60) snprintf(mn, sizeof(mn), "%dh%02d", mins / 60, mins % 60);
      else snprintf(mn, sizeof(mn), "%dm", mins);
    }
    snprintf(etaBuf, sizeof(etaBuf), "%s · %s", mn, km);
  } else if (!targetNav.valid) {
    snprintf(etaBuf, sizeof(etaBuf), bleConnected ? "BLE READY" : "STANDBY");
  }

  if (force || strcmp(etaBuf, lastEtaText) != 0) {
    strncpy(lastEtaText, etaBuf, sizeof(lastEtaText) - 1);
    lastEtaText[sizeof(lastEtaText) - 1] = '\0';
    tft.fillRect(px + 66, py + 6, 102, 18, card);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(targetNav.offRoute ? mapsColDanger() : mapsColText(), card);
    tft.drawString(etaBuf, px + 116, py + 15, 2);
    tft.setTextDatum(TL_DATUM);
  }

  // 4. Clock / Battery / GPS meta line (dirty-checked)
  char metaBuf[28];
  if (targetNav.timeStr[0] != '\0' && targetNav.phoneBattery >= 0) {
    snprintf(metaBuf, sizeof(metaBuf), "%s  BAT:%d%%", targetNav.timeStr, targetNav.phoneBattery);
  } else if (targetNav.phoneBattery >= 0) {
    snprintf(metaBuf, sizeof(metaBuf), "HP:%d%%  %s", targetNav.phoneBattery, bleConnected ? "BLE" : "NO BLE");
  } else if (targetNav.timeStr[0] != '\0') {
    snprintf(metaBuf, sizeof(metaBuf), "%s  %s", targetNav.timeStr, bleConnected ? "BLE OK" : "NO BLE");
  } else if (hwGps.fixValid) {
    snprintf(metaBuf, sizeof(metaBuf), "GPS:%dSAT  %s", hwGps.satellites, bleConnected ? "BLE" : "OFF");
  } else {
    snprintf(metaBuf, sizeof(metaBuf), "%s", bleConnected ? "BLE LINKED" : "DISCONNECTED");
  }

  if (force || strcmp(metaBuf, lastMetaText) != 0) {
    strncpy(lastMetaText, metaBuf, sizeof(lastMetaText) - 1);
    lastMetaText[sizeof(lastMetaText) - 1] = '\0';
    tft.fillRect(px + 66, py + 25, 102, 12, card);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(mapsColMuted(), card);
    tft.drawString(metaBuf, px + 116, py + 31, 1);
    tft.setTextDatum(TL_DATUM);
  }
}

void MapRenderer::drawStreetsInto(TFT_eSPI& dst, int16_t offsetY) {
  if (curStreetCount <= 0) return;

  const uint16_t land = mapsColLand();
  const uint16_t edge = mapsColRoadEdge();
  const uint16_t fill = mapsColRoadFill();
  const float edgeR = STREET_EDGE_W * 0.5f;
  const float fillR = STREET_FILL_W * 0.5f;
  const float bandMin = static_cast<float>(offsetY - 12);
  const float bandMax = static_cast<float>(offsetY + STRIP_H + 12);

  for (int pass = 0; pass < 2; pass++) {
    const uint16_t col = (pass == 0) ? edge : fill;
    const uint16_t bg  = (pass == 0) ? land : edge;
    const float wd     = (pass == 0) ? STREET_EDGE_W : STREET_FILL_W;
    const float rad    = (pass == 0) ? edgeR : fillR;

    for (int i = 0; i < curStreetCount; i++) {
      const float p1x = curStreetX0[i];
      const float p1y = curStreetY0[i];
      const float p2x = curStreetX1[i];
      const float p2y = curStreetY1[i];

      // Band bounding-box cull
      const float segMinY = (p1y < p2y) ? p1y : p2y;
      const float segMaxY = (p1y > p2y) ? p1y : p2y;
      if (segMaxY < bandMin || segMinY > bandMax) continue;

      // Check chain continuity for Catmull-Rom curve smoothing
      float p0x = p1x, p0y = p1y;
      bool hasPrev = false;
      if (i > 0 && fabsf(curStreetX1[i - 1] - p1x) < 2.5f && fabsf(curStreetY1[i - 1] - p1y) < 2.5f) {
        p0x = curStreetX0[i - 1];
        p0y = curStreetY0[i - 1];
        hasPrev = true;
      }

      float p3x = p2x, p3y = p2y;
      bool hasNext = false;
      if (i + 1 < curStreetCount && fabsf(p2x - curStreetX0[i + 1]) < 2.5f && fabsf(p2y - curStreetY0[i + 1]) < 2.5f) {
        p3x = curStreetX1[i + 1];
        p3y = curStreetY1[i + 1];
        hasNext = true;
      }

      if (!hasPrev && !hasNext) {
        dst.drawWideLine(p1x, p1y - offsetY, p2x, p2y - offsetY, wd, col, bg);
        dst.drawSpot(p1x, p1y - offsetY, rad, col, bg);
        dst.drawSpot(p2x, p2y - offsetY, rad, col, bg);
      } else {
        const int steps = 4;
        float prevX = p1x;
        float prevY = p1y;
        dst.drawSpot(prevX, prevY - offsetY, rad, col, bg);
        for (int s = 1; s <= steps; s++) {
          float nx, ny;
          catmullRomPoint(p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y,
                          static_cast<float>(s) / steps, nx, ny);
          dst.drawWideLine(prevX, prevY - offsetY, nx, ny - offsetY, wd, col, bg);
          dst.drawSpot(nx, ny - offsetY, rad, col, bg);
          prevX = nx;
          prevY = ny;
        }
      }
    }
  }
}

void MapRenderer::drawRouteInto(TFT_eSPI& dst, int16_t offsetY) {
  if (curRouteCount < 2) return;

  const uint16_t land    = mapsColLand();
  const uint16_t edgeCol = targetNav.offRoute ? mapsColCardShadow() : mapsColRouteEdge();
  const uint16_t fillCol = targetNav.offRoute ? mapsColDanger() : mapsColRoute();
  const float edgeR = ROUTE_EDGE_W * 0.5f;
  const float fillR = ROUTE_FILL_W * 0.5f;
  const float bandMin = static_cast<float>(offsetY - 14);
  const float bandMax = static_cast<float>(offsetY + STRIP_H + 14);

  for (int pass = 0; pass < 2; pass++) {
    const uint16_t col = (pass == 0) ? edgeCol : fillCol;
    const uint16_t bg  = (pass == 0) ? land : edgeCol;
    const float wd     = (pass == 0) ? ROUTE_EDGE_W : ROUTE_FILL_W;
    const float rad    = (pass == 0) ? edgeR : fillR;

    for (int i = 0; i < curRouteCount - 1; i++) {
      const float p1x = curRouteX[i];
      const float p1y = curRouteY[i];
      const float p2x = curRouteX[i + 1];
      const float p2y = curRouteY[i + 1];
      if (p1x < 0.0f || p1y < 0.0f || p2x < 0.0f || p2y < 0.0f) continue;

      const float segMinY = (p1y < p2y) ? p1y : p2y;
      const float segMaxY = (p1y > p2y) ? p1y : p2y;
      if (segMaxY < bandMin || segMinY > bandMax) continue;

      const int i0 = (i > 0) ? (i - 1) : i;
      const int i3 = (i + 2 < curRouteCount) ? (i + 2) : (i + 1);
      const float p0x = (curRouteX[i0] >= 0.0f) ? curRouteX[i0] : p1x;
      const float p0y = (curRouteY[i0] >= 0.0f) ? curRouteY[i0] : p1y;
      const float p3x = (curRouteX[i3] >= 0.0f) ? curRouteX[i3] : p2x;
      const float p3y = (curRouteY[i3] >= 0.0f) ? curRouteY[i3] : p2y;

      const int steps = 5;
      float prevX = p1x;
      float prevY = p1y;
      dst.drawSpot(prevX, prevY - offsetY, rad, col, bg);
      for (int s = 1; s <= steps; s++) {
        float nx, ny;
        catmullRomPoint(p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y,
                        static_cast<float>(s) / steps, nx, ny);
        dst.drawWideLine(prevX, prevY - offsetY, nx, ny - offsetY, wd, col, bg);
        dst.drawSpot(nx, ny - offsetY, rad, col, bg);
        prevX = nx;
        prevY = ny;
      }
    }
  }

  // Destination beacon at index 0
  const float dx = curRouteX[0];
  const float dy = curRouteY[0] - offsetY;
  if (dx >= 0.0f && dx < SCR_W && dy >= -8.0f && dy <= STRIP_H + 8.0f) {
    dst.drawSpot(dx, dy, 5.5f, mapsColWhite(), edgeCol);
    dst.drawSpot(dx, dy, 3.0f, mapsColAccent(), mapsColWhite());
  }
}

void MapRenderer::drawUserMarkerInto(TFT_eSPI& dst, int16_t offsetY) {
  if (!targetNav.hasUserPosition) return;
  const float localY = curUserY - offsetY;
  if (localY < -20.0f || localY > STRIP_H + 20.0f) return;

  const int16_t ix = static_cast<int16_t>(roundf(curUserX));
  const int16_t iy = static_cast<int16_t>(roundf(localY));

  dst.drawSmoothArc(ix, iy, 16, 14, 0, 360, mapsColMonoEye(), mapsColLand(), false);

  if (chevronReady && (static_cast<const TFT_eSPI*>(&dst) == static_cast<const TFT_eSPI*>(&mapStripSpr))) {
    mapStripSpr.setPivot(ix, iy);
    chevronSpr.pushRotated(&mapStripSpr, static_cast<int16_t>(roundf(curBearing)), TFT_TRANSPARENT);
    mapStripSpr.setPivot(0, 0);
  } else if (chevronReady) {
    tft.setPivot(ix, iy);
    chevronSpr.pushRotated(static_cast<int16_t>(roundf(curBearing)), TFT_TRANSPARENT);
    tft.setPivot(0, 0);
  }
}

void MapRenderer::renderMapViewportBands() {
  if (!mapStripReady) return;

  const uint16_t land = mapsColLand();
  const float turnEase = easeOutCubic(turnAnimT);
  const int16_t turnCardY = static_cast<int16_t>(roundf(-36.0f + 66.0f * turnEase)); // Slides down to y = 30

  const float toastEase = easeOutCubic(toastAnimT);
  const int16_t toastY = static_cast<int16_t>(roundf(260.0f - 66.0f * toastEase));  // Slides up from bottom to y = 194

  for (int b = 0; b < STRIP_CNT; b++) {
    const int16_t bandY = MAP_TOP_Y + b * STRIP_H;
    mapStripSpr.fillSprite(land);

    drawMapBackdropInto(mapStripSpr, bandY);
    drawStreetsInto(mapStripSpr, bandY);
    drawRouteInto(mapStripSpr, bandY);
    drawUserMarkerInto(mapStripSpr, bandY);

    if (turnCardY + 58 > bandY && turnCardY < bandY + STRIP_H) {
      drawFloatingTurnCardInto(mapStripSpr, turnCardY, bandY);
    }
    if (toastActive && (toastY + 58 > bandY && toastY < bandY + STRIP_H)) {
      drawToastInto(mapStripSpr, toastY, bandY);
    }

    mapStripSpr.pushSprite(0, bandY);
  }
}

void MapRenderer::draw(const NavState& state, const HwGpsData& hwGps, const MediaState& media, bool bleConnected) {
  setTargetNav(state);
  const DisplayViewMode mode = motoViewModeGet();
  if (mode == VIEW_MEDIA_PLAYER) {
    drawMediaPlayerMode(state, hwGps, media, bleConnected);
  } else if (mode == VIEW_MOTO_HUD) {
    drawMotoHudMode(state, hwGps, bleConnected);
  } else {
    drawVectorMode(state, hwGps, bleConnected);
  }
}

void MapRenderer::drawVectorMode(const NavState& state, const HwGpsData& hwGps, bool bleConnected) {
  (void)state;
  drawTopNavButtons(bleConnected, &hwGps);
  topBarDrawn = true;
  lastBleStatus = bleConnected;
  lastSatCount = hwGps.satellites;

  renderMapViewportBands();
  mapDirty = false;

  drawBottomTelemetryFrame(true);
  updateBottomTelemetryDynamic(hwGps, bleConnected, true);
}

void MapRenderer::updateMotoHudDynamic(const HwGpsData& hwGps, bool bleConnected) {
  (void)hwGps;
  (void)bleConnected;
  const uint16_t card = mapsColCard();
  const int distInt = static_cast<int>(roundf(curDistanceM));

  char distLine[20];
  if (targetNav.offRoute) {
    snprintf(distLine, sizeof(distLine), "OFF ROUTE!");
  } else if (distInt >= 1000) {
    snprintf(distLine, sizeof(distLine), "%.1f KM", distInt / 1000.0f);
  } else {
    snprintf(distLine, sizeof(distLine), "%d M", distInt);
  }

  if (strcmp(distLine, lastHudDist) != 0) {
    strncpy(lastHudDist, distLine, sizeof(lastHudDist) - 1);
    lastHudDist[sizeof(lastHudDist) - 1] = '\0';
    tft.fillRect(42, 136, SCR_W - 84, 28, card);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(targetNav.offRoute ? mapsColDanger() : mapsColText(), card);
    tft.drawString(distLine, SCR_W / 2, 150, 4);
    tft.setTextDatum(TL_DATUM);
  }
}

void MapRenderer::drawMotoHudMode(const NavState& state, const HwGpsData& hwGps, bool bleConnected) {
  clearMapArea();
  drawTopNavButtons(bleConnected, &hwGps);
  topBarDrawn = true;

  const uint16_t card = mapsColCard();
  drawFloatingCard(tft, 12, 34, SCR_W - 24, 138, 16, card,
                   state.offRoute ? mapsColDanger() : mapsColCardBorder(), mapsColLand());

  if (state.hasChronosIcon) {
    const uint16_t iconCol = state.offRoute ? mapsColDanger() : mapsColAccent();
    tft.drawSmoothArc(SCR_W / 2, 86, 46, 44, 0, 360, mapsColCardBorder(), card, false);
    const int16_t ox = (SCR_W / 2) - 44;
    const int16_t oy = 42;
    for (int sy = 2; sy < 46; sy++) {
      for (int sx = 2; sx < 46; sx++) {
        const int byteIdx = (sy * 48 + sx) / 8;
        const int bitPos = 7 - (sx % 8);
        if ((state.chronosIcon[byteIdx] >> bitPos) & 0x01) {
          tft.fillRect(ox + (sx - 2) * 2, oy + (sy - 2) * 2, 2, 2, iconCol);
        }
      }
    }
  } else {
    drawBigManeuverArrow(SCR_W / 2, 88, state.instruction, card);
  }
  lastHudDist[0] = '\0';
  updateMotoHudDynamic(hwGps, bleConnected);

  drawFloatingCard(tft, 12, 180, SCR_W - 24, 68, 12, card, mapsColCardBorder(), mapsColLand());
  if (state.instruction[0] != '\0') {
    drawUtf8Text(tft, state.instruction, 22, 190, SCR_W - 44, mapsColText(), card);
  }
  if (state.street[0] != '\0') {
    drawUtf8Text(tft, state.street, 22, 216, SCR_W - 44, mapsColMuted(), card);
  }

  if (toastActive) {
    drawToastInto(tft, 194, 0);
  }

  drawBottomTelemetryFrame(true);
  updateBottomTelemetryDynamic(hwGps, bleConnected, true);
}

void MapRenderer::drawMediaPlayerMode(const NavState& state, const HwGpsData& hwGps,
                                      const MediaState& media, bool bleConnected) {
  (void)state;
  clearMapArea();
  drawTopNavButtons(bleConnected, &hwGps);
  topBarDrawn = true;

  const uint16_t card = mapsColCard();
  const uint16_t border = mapsColCardBorder();
  const uint16_t bg = mapsColLand();

  drawFloatingCard(tft, 8, 34, SCR_W - 16, 76, 12, card, border, bg);
  tft.fillSmoothRoundRect(14, 42, 4, 60, 2, mapsColMonoEye(), card);

  const char* appName = (media.valid && media.app[0] != '\0') ? media.app : "HP AUDIO";
  const char* playBadge = (!bleConnected)
    ? "NO BLE"
    : (!media.valid
        ? "READY"
        : (media.state == MEDIA_PLAYING ? "PLAYING" : (media.state == MEDIA_PAUSED ? "PAUSED" : "STOPPED")));
  const uint16_t badgeCol = (!bleConnected)
    ? mapsColDanger()
    : (media.state == MEDIA_PLAYING ? mapsColSuccess() : (media.state == MEDIA_PAUSED ? mapsColAccent() : mapsColMuted()));

  tft.setTextColor(mapsColMuted(), card);
  tft.drawString(appName, 24, 43, 1);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(badgeCol, card);
  tft.drawString(playBadge, SCR_W - 18, 43, 1);
  tft.setTextDatum(TL_DATUM);

  const char* titleTxt = (media.valid && media.title[0] != '\0')
    ? media.title
    : (bleConnected ? "Siap Kontrol Musik HP" : "Hubungkan BLE App");
  const char* artistTxt = (media.valid && media.artist[0] != '\0')
    ? media.artist
    : "Tap Play / Next di layar CYD";

  drawUtf8Text(tft, titleTxt, 24, 57, SCR_W - 40, mapsColText(), card);
  drawUtf8Text(tft, artistTxt, 24, 81, SCR_W - 40, mapsColMuted(), card);

  // Row 1 Buttons: PREV | PLAY/PAUSE | NEXT
  drawFloatingCard(tft, 8, 118, 70, 58, 12, card, border, bg);
  tft.drawWideLine(28, 134, 28, 150, 3.0f, mapsColText(), card);
  tft.fillTriangle(50, 134, 50, 150, 33, 142, mapsColText());
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(mapsColMuted(), card);
  tft.drawString("PREV", 43, 165, 1);

  const bool isPlaying = (media.valid && media.state == MEDIA_PLAYING);
  drawFloatingCard(tft, 84, 118, 72, 58, 12, card, isPlaying ? mapsColAccent() : mapsColMonoEye(), bg);
  if (isPlaying) {
    tft.fillSmoothRoundRect(110, 132, 6, 20, 2, mapsColAccent(), card);
    tft.fillSmoothRoundRect(124, 132, 6, 20, 2, mapsColAccent(), card);
  } else {
    tft.fillTriangle(111, 130, 111, 154, 133, 142, mapsColMonoEye());
  }
  tft.setTextColor(isPlaying ? mapsColAccent() : mapsColMonoEye(), card);
  tft.drawString(isPlaying ? "PAUSE" : "PLAY", 120, 165, 1);

  drawFloatingCard(tft, 162, 118, 70, 58, 12, card, border, bg);
  tft.fillTriangle(184, 134, 184, 150, 201, 142, mapsColText());
  tft.drawWideLine(205, 134, 205, 150, 3.0f, mapsColText(), card);
  tft.setTextColor(mapsColMuted(), card);
  tft.drawString("NEXT", 197, 165, 1);

  // Row 2 Buttons: VOL - | STOP | VOL +
  drawFloatingCard(tft, 8, 184, 70, 54, 12, card, border, bg);
  tft.setTextColor(mapsColText(), card);
  tft.drawString("VOL -", 43, 211, 2);

  drawFloatingCard(tft, 84, 184, 72, 54, 12, card, mapsColDanger(), bg);
  tft.fillSmoothRoundRect(113, 195, 14, 14, 3, mapsColDanger(), card);
  tft.setTextColor(mapsColDanger(), card);
  tft.drawString("STOP", 120, 224, 1);

  drawFloatingCard(tft, 162, 184, 70, 54, 12, card, border, bg);
  tft.setTextColor(mapsColText(), card);
  tft.drawString("VOL +", 197, 211, 2);
  tft.setTextDatum(TL_DATUM);

  drawBottomTelemetryFrame(true);
  updateBottomTelemetryDynamic(hwGps, bleConnected, true);
}

void MapRenderer::drawStandaloneGpsScreen(const HwGpsData& hwGps, bool bleConnected) {
  clearMapArea();
  drawTopNavButtons(bleConnected, &hwGps);

  const uint16_t card = mapsColCard();
  const uint16_t bg = mapsColLand();
  const uint16_t track = mapsColCardAlt();

  drawFloatingCard(tft, 20, 38, SCR_W - 40, 144, 18, card, mapsColCardBorder(), bg);

  const int16_t cx = SCR_W / 2;
  const int16_t cy = 108;
  tft.drawSmoothArc(cx, cy, 52, 45, 45, 315, track, card, true);

  const float spd = hwGps.fixValid ? hwGps.speedKmh : 0.0f;
  const float ratio = clampf(spd / 120.0f, 0.0f, 1.0f);
  const uint16_t endAng = static_cast<uint16_t>(45 + ratio * 270.0f);
  if (endAng > 47) {
    tft.drawSmoothArc(cx, cy, 52, 45, 45, endAng, mapsColMonoEye(), track, true);
  }

  char spdBuf[12];
  snprintf(spdBuf, sizeof(spdBuf), "%d", static_cast<int>(roundf(spd)));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(mapsColText(), card);
  tft.drawString(spdBuf, cx, cy - 2, 6);
  tft.setTextColor(mapsColAccent(), card);
  tft.drawString("KM/H", cx, cy + 34, 2);

  drawFloatingCard(tft, 20, 192, SCR_W - 40, 72, 14, card, mapsColCardBorder(), bg);
  tft.setTextColor(bleConnected ? mapsColSuccess() : mapsColMonoEye(), card);
  tft.drawString(bleConnected ? "BLE: TERHUBUNG" : "BLE: MENUNGGU APP", cx, 214, 2);

  char gpsInfo[40];
  if (hwGps.fixValid) {
    snprintf(gpsInfo, sizeof(gpsInfo), "GPS LOCK (%d SAT) · %.0fm", hwGps.satellites, hwGps.altitudeM);
    tft.setTextColor(mapsColSuccess(), card);
  } else {
    snprintf(gpsInfo, sizeof(gpsInfo), "SCANNING GPS (%d SAT)", hwGps.satellites);
    tft.setTextColor(mapsColMuted(), card);
  }
  tft.drawString(gpsInfo, cx, 242, 2);
  tft.setTextDatum(TL_DATUM);

  drawThemeSwitchWaiting();
}
