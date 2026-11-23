#include "loading_screen.h"
#include "nav_types.h"
#include "maps_theme.h"
#include "utf8_text.h"
#include <math.h>
#include <stdio.h>

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.01745329251f
#endif

// Compact 168x88 16-bit Sprite (~29.5 KB) allocated only during boot/loading for 60 FPS flicker-free sweep
static const int16_t SPR_W = 168;
static const int16_t SPR_H = 88;
static const int16_t SPR_X = (SCR_W - SPR_W) / 2;
static const int16_t SPR_Y = 62;

static TFT_eSprite* s_loadSpr = nullptr;
static uint32_t s_lastFrameMs = 0;
static uint32_t s_startMs = 0;

static void ensureLoadingSprite(TFT_eSPI& tft) {
  if (s_loadSpr == nullptr) {
    s_loadSpr = new TFT_eSprite(&tft);
    if (s_loadSpr != nullptr) {
      s_loadSpr->setColorDepth(16);
      if (s_loadSpr->createSprite(SPR_W, SPR_H) == nullptr) {
        s_loadSpr->deleteSprite();
        delete s_loadSpr;
        s_loadSpr = nullptr;
      }
    }
  }
}

void freeMapLoadingSprite() {
  if (s_loadSpr != nullptr) {
    s_loadSpr->deleteSprite();
    delete s_loadSpr;
    s_loadSpr = nullptr;
  }
}

static inline float easeOutCubic(float t) {
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  const float inv = 1.0f - t;
  return 1.0f - inv * inv * inv;
}

template <typename TargetT>
static void drawClusterGaugeContent(TargetT& dst, int16_t ox, int16_t oy,
                                    float sweep01, float trackGlow01, const char* subLabel) {
  const uint16_t bgCol     = mapsColLand();
  const uint16_t cardCol   = mapsColCard();
  const uint16_t trackCol  = mapsColCardAlt();
  const uint16_t borderCol = mapsColCardBorder();
  const uint16_t blueCol   = mapsColMonoEye();
  const uint16_t orangeCol = mapsColAccent();
  const uint16_t dangerCol = mapsColDanger();

  const int16_t cx = SPR_W / 2;
  const int16_t cy = 43;
  const int16_t rOuter = 36;
  const int16_t rInner = 29;

  // TFT_eSPI drawSmoothArc angle convention: 0=6h, 90=9h, 180=12h, 270=3h
  const uint16_t arcStart = 45;
  const uint16_t arcEnd   = 315;
  const uint16_t curAngle = static_cast<uint16_t>(arcStart + sweep01 * (arcEnd - arcStart));

  dst.fillSmoothRoundRect(ox, oy, SPR_W, SPR_H, 14, cardCol, bgCol);
  dst.drawSmoothRoundRect(ox, oy, 14, 13, SPR_W, SPR_H, borderCol, cardCol);

  // Base RPM track + redline band
  dst.drawSmoothArc(ox + cx, oy + cy, rOuter, rInner, arcStart, arcEnd, trackCol, cardCol, true);
  dst.drawSmoothArc(ox + cx, oy + cy, rOuter, rInner + 2, 265, arcEnd, dangerCol, trackCol, true);

  // Illuminated active RPM sweep arc
  if (curAngle > arcStart + 2) {
    const uint16_t activeCol = (curAngle > 265) ? orangeCol : blueCol;
    dst.drawSmoothArc(ox + cx, oy + cy, rOuter, rInner, arcStart, curAngle, activeCol, trackCol, true);
  }

  // Crisp anti-aliased JDM gauge needle
  const float needleDeg = static_cast<float>(curAngle) + 90.0f;
  const float nRad = needleDeg * DEG_TO_RAD;
  const int16_t nx = ox + cx + static_cast<int16_t>(cosf(nRad) * 26.0f);
  const int16_t ny = oy + cy + static_cast<int16_t>(sinf(nRad) * 26.0f);
  dst.drawWideLine(ox + cx, oy + cy, nx, ny, 2.8f, orangeCol, cardCol);
  dst.drawSpot(ox + cx, oy + cy, 4.5f, mapsColText(), cardCol);
  dst.drawSpot(ox + cx, oy + cy, 2.0f, orangeCol, mapsColText());

  // Digital readout below hub
  dst.setTextDatum(MC_DATUM);
  dst.setTextColor(mapsColText(), cardCol);
  dst.drawString(subLabel, ox + cx, oy + cy + 18, 1);

  // Illuminated horizontal light-trail progress bar at bottom of sprite
  const int16_t barX = ox + 18;
  const int16_t barY = oy + SPR_H - 11;
  const int16_t barW = SPR_W - 36;
  dst.fillSmoothRoundRect(barX, barY, barW, 4, 2, trackCol, cardCol);
  const int16_t dotX = barX + static_cast<int16_t>(trackGlow01 * (barW - 18));
  dst.fillSmoothRoundRect(barX, barY, (dotX - barX) + 18, 4, 2, blueCol, trackCol);
  dst.drawSpot(dotX + 16, barY + 2, 2.5f, orangeCol, cardCol);
  dst.setTextDatum(TL_DATUM);
}

static void renderClusterGaugeFrame(TFT_eSPI& tft, float sweep01, float trackGlow01, const char* subLabel) {
  const uint16_t bgCol = mapsColLand();
  if (s_loadSpr != nullptr) {
    s_loadSpr->fillSprite(bgCol);
    drawClusterGaugeContent(*s_loadSpr, 0, 0, sweep01, trackGlow01, subLabel);
    s_loadSpr->pushSprite(SPR_X, SPR_Y);
  } else {
    drawClusterGaugeContent(tft, SPR_X, SPR_Y, sweep01, trackGlow01, subLabel);
  }
}

void playBootSplashSweep(TFT_eSPI& tft) {
  tft.fillScreen(mapsColLand());

  // Top badge
  tft.fillSmoothRoundRect(34, 20, SCR_W - 68, 22, 11, mapsColCard(), mapsColLand());
  tft.drawSmoothRoundRect(34, 20, 11, 10, SCR_W - 68, 22, mapsColCardBorder(), mapsColCard());
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(mapsColMonoEye(), mapsColCard());
  tft.drawString("DISPLAYCONNECT MOTO", SCR_W / 2, 31, 1);

  // Bottom splash subtitle
  tft.fillSmoothRoundRect(24, 174, SCR_W - 48, 56, 14, mapsColCard(), mapsColLand());
  tft.drawSmoothRoundRect(24, 174, 14, 13, SCR_W - 48, 56, mapsColCardBorder(), mapsColCard());
  tft.setTextColor(mapsColText(), mapsColCard());
  tft.drawString("CLUSTER SELF-TEST", SCR_W / 2, 194, 2);
  tft.setTextColor(mapsColMuted(), mapsColCard());
  tft.drawString("60 FPS GRAPHICS ENGINE", SCR_W / 2, 214, 1);
  tft.setTextDatum(TL_DATUM);

  ensureLoadingSprite(tft);

  // Quick 650ms JDM needle sweep up (0->1) then smooth settle back (1->0)
  const uint32_t t0 = millis();
  while (true) {
    const uint32_t elapsed = millis() - t0;
    if (elapsed >= 660) break;

    float sweep = 0.0f;
    float track = 0.0f;
    if (elapsed < 340) {
      const float p = static_cast<float>(elapsed) / 340.0f;
      sweep = easeOutCubic(p);
      track = sweep;
    } else {
      const float p = static_cast<float>(elapsed - 340) / 320.0f;
      sweep = 1.0f - easeOutCubic(p);
      track = 1.0f;
    }

    char rpmBuf[16];
    snprintf(rpmBuf, sizeof(rpmBuf), "%d RPM", static_cast<int>(roundf(sweep * 12.0f)) * 1000);
    renderClusterGaugeFrame(tft, sweep, track, rpmBuf);
    delay(16); // ~60 FPS
    yield();
  }

  freeMapLoadingSprite();
}

void showMapLoadingScreen(TFT_eSPI& tft) {
  s_lastFrameMs = 0;
  s_startMs = millis();

  tft.fillScreen(mapsColLand());

  tft.fillSmoothRoundRect(36, 18, SCR_W - 72, 24, 12, mapsColCard(), mapsColLand());
  tft.drawSmoothRoundRect(36, 18, 12, 11, SCR_W - 72, 24, mapsColCardBorder(), mapsColCard());
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(mapsColMonoEye(), mapsColCard());
  tft.drawString("MOTO HUD // SYNC", SCR_W / 2, 30, 1);

  tft.fillSmoothRoundRect(18, 174, SCR_W - 32, 92, 14, mapsColCardShadow(), mapsColLand());
  tft.fillSmoothRoundRect(16, 172, SCR_W - 32, 92, 14, mapsColCard(), mapsColLand());
  tft.drawSmoothRoundRect(16, 172, 14, 13, SCR_W - 32, 92, mapsColCardBorder(), mapsColCard());

  const DisplayLang l = displayLangGet();
  const char* msg1 = (l == LANG_ID) ? "MENGUNCI RUTE GPS..." : (l == LANG_EN ? "ACQUIRING ROUTE..." : "BUSCANDO ROTA...");
  const char* msg2 = (l == LANG_ID) ? "SINKRONISASI PETA & TELEMETRI" : (l == LANG_EN ? "SYNCING MAP & TELEMETRY" : "SINCRONIZANDO MAPA");

  tft.setTextColor(mapsColText(), mapsColCard());
  tft.drawString(msg1, SCR_W / 2, 202, 2);
  tft.setTextColor(mapsColMuted(), mapsColCard());
  tft.drawString(msg2, SCR_W / 2, 228, 1);

  tft.drawSpot(SCR_W / 2 - 14, 248, 2.5f, mapsColMonoEye(), mapsColCard());
  tft.drawSpot(SCR_W / 2,      248, 2.5f, mapsColAccent(), mapsColCard());
  tft.drawSpot(SCR_W / 2 + 14, 248, 2.5f, mapsColSuccess(), mapsColCard());
  tft.setTextDatum(TL_DATUM);

  ensureLoadingSprite(tft);
  renderClusterGaugeFrame(tft, 0.0f, 0.0f, "SYNC 0%");
}

void updateMapLoadingAnimation(TFT_eSPI& tft, uint32_t nowMs) {
  if (nowMs - s_lastFrameMs < 16) return; // ~60 FPS smooth sprite sweep
  s_lastFrameMs = nowMs;

  const uint32_t elapsed = nowMs - s_startMs;
  const float rad = (static_cast<float>(elapsed) * 0.0036f);
  const float sweep = 0.5f - 0.5f * cosf(rad);
  const float track = 0.5f + 0.5f * sinf(rad * 0.8f);

  char buf[16];
  snprintf(buf, sizeof(buf), "SYNC %d%%", static_cast<int>(roundf(sweep * 100.0f)));
  renderClusterGaugeFrame(tft, sweep, track, buf);
}
