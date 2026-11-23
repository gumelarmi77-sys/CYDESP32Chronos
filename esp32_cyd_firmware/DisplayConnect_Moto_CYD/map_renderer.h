#pragma once
#include <TFT_eSPI.h>
#include "nav_types.h"

class MapRenderer {
public:
  explicit MapRenderer(TFT_eSPI& display);
  void begin();
  void refreshThemeAssets();
  void invalidateLayout();

  // Push new BLE/GPS targets (interpolated smoothly every frame by tick())
  void setTargetNav(const NavState& state);
  void triggerNotifyToast(const NotifyState& notify);
  void dismissNotifyToast();
  bool isToastVisible() const { return toastActive; }

  // 60+ FPS frame-rate independent physics + dirty-rect/strip compositor
  void tick(float dt, uint32_t nowMs, const HwGpsData& hwGps, bool bleConnected, bool hasNav);

  // Immediate full render (mode switch / theme switch / initial draw)
  void draw(const NavState& state, const HwGpsData& hwGps, const MediaState& media, bool bleConnected = true);
  void drawMediaPlayerMode(const NavState& state, const HwGpsData& hwGps, const MediaState& media, bool bleConnected);
  void drawNotifyBanner(const NotifyState& notify);
  void drawStandaloneGpsScreen(const HwGpsData& hwGps, bool bleConnected);
  void clearMapArea();
  void drawThemeSwitch();
  void drawThemeSwitchWaiting();
  void drawTopNavButtons(bool bleConnected = false, const HwGpsData* hwGps = nullptr);
  void drawFloatingCard(TFT_eSPI& dst, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                        uint16_t fill, uint16_t border, uint16_t bg);

  // Touch hit zones (glove-friendly sizes)
  static bool themeSwitchHit(uint16_t x, uint16_t y);
  static bool viewModeSwitchHit(uint16_t x, uint16_t y);
  static bool brightnessSwitchHit(uint16_t x, uint16_t y);
  static bool notifyDismissHit(uint16_t x, uint16_t y);
  static MediaCommand mediaControlHit(uint16_t x, uint16_t y);

private:
  TFT_eSPI& tft;
  TFT_eSprite chevronSpr;   // 24x24   (1,152 B)  - 3D rotating motor chevron
  TFT_eSprite digitCellSpr; // 16x28   (896 B)    - Tabular digit dirty cell
  TFT_eSprite barSpr;       // 96x8    (1,536 B)  - Peak-hold speed/RPM bar
  TFT_eSprite mapStripSpr;  // 240x38  (18,240 B) - Flicker-free strip compositor

  bool chevronReady   = false;
  bool digitCellReady = false;
  bool barReady       = false;
  bool mapStripReady  = false;

  // Frame-rate independent physics state (targets vs smoothly interpolated current values)
  NavState targetNav;
  bool hasTarget = false;

  double curLat = 0.0;
  double curLon = 0.0;
  float curBearing   = 0.0f;
  float curSpeedKmh  = 0.0f;
  float peakSpeedKmh = 0.0f;
  float peakHoldTimer = 0.0f;
  float peakFallVel   = 0.0f;
  float curDistanceM  = 0.0f;
  float curUserX      = 120.0f;
  float curUserY      = 170.0f;

  int curRouteCount = 0;
  float curRouteX[NAV_MAX_ROUTE_POINTS] = {};
  float curRouteY[NAV_MAX_ROUTE_POINTS] = {};

  int curStreetCount = 0;
  float curStreetX0[NAV_MAX_STREET_SEGMENTS] = {};
  float curStreetY0[NAV_MAX_STREET_SEGMENTS] = {};
  float curStreetX1[NAV_MAX_STREET_SEGMENTS] = {};
  float curStreetY1[NAV_MAX_STREET_SEGMENTS] = {};

  // Cubic ease-out UI transition timers
  float turnAnimT = 1.0f;
  char prevManeuverSig[96] = {};

  NotifyState toastState;
  bool toastActive  = false;
  bool toastClosing = false;
  float toastAnimT  = 0.0f;

  // Dirty-checking caches for zero-flicker partial updates
  bool topBarDrawn          = false;
  bool bottomPillFrameDrawn = false;
  bool hudCardsDrawn        = false;
  bool hudStaticDirty       = true;
  bool mapDirty             = true;
  bool lastBleStatus        = false;
  uint8_t lastSatCount      = 255;
  char lastSpeedChars[4]    = { ' ', ' ', ' ', '\0' };
  char lastEtaText[28]      = {};
  char lastMetaText[28]     = {};
  char lastHudDist[20]      = {};
  int16_t lastBarFillPx     = -1;
  int16_t lastBarPeakPx     = -1;

  void buildChevronSprite();
  void drawMapBackdropInto(TFT_eSPI& dst, int16_t offsetY);
  void drawStreetsInto(TFT_eSPI& dst, int16_t offsetY);
  void drawRouteInto(TFT_eSPI& dst, int16_t offsetY);
  void drawUserMarkerInto(TFT_eSPI& dst, int16_t offsetY);
  void drawFloatingTurnCardInto(TFT_eSPI& dst, int16_t cardY, int16_t offsetY);
  void drawToastInto(TFT_eSPI& dst, int16_t toastY, int16_t offsetY);

  void renderMapViewportBands();
  void drawBottomTelemetryFrame(bool force);
  void updateBottomTelemetryDynamic(const HwGpsData& hwGps, bool bleConnected, bool force);
  void drawTabularSpeedDigits(int speedInt, uint16_t fg, uint16_t bg, bool force);
  void drawPeakHoldSpeedBar(float spd, float peak, uint16_t bg, bool force);

  void drawVectorMode(const NavState& state, const HwGpsData& hwGps, bool bleConnected);
  void drawMotoHudMode(const NavState& state, const HwGpsData& hwGps, bool bleConnected);
  void updateMotoHudDynamic(const HwGpsData& hwGps, bool bleConnected);

  void drawMiniManeuverArrow(TFT_eSPI& dst, int16_t cx, int16_t cy, const char* instruction, uint16_t col, uint16_t bg);
  void drawBigManeuverArrow(int16_t cx, int16_t cy, const char* instruction, uint16_t bg);
  void drawThemeSwitchAt(int sx, int sy, uint16_t bg);
  void drawHudButtonTopRight(uint16_t bg);
  void drawBriButtonTopLeft(uint16_t bg);
  void drawTopStatusPill(bool bleConnected, const HwGpsData* hwGps, uint16_t bg);
};
