#pragma once
#include <stdint.h>

#define NAV_MAX_ROUTE_POINTS    64
#define NAV_MAX_STREET_SEGMENTS 128

#define SCR_W      240
#define SCR_H      320
#define MAP_AREA_H 232
#define OVERLAY_H  (SCR_H - MAP_AREA_H)

#define NAV_HTML_MAX 480

enum DisplayLang : uint8_t {
  LANG_ID = 0,
  LANG_EN = 1,
  LANG_PT = 2,
};

enum DisplayViewMode : uint8_t {
  VIEW_MAP_VECTOR   = 0, // Modern automotive vector map + floating cards
  VIEW_MOTO_HUD     = 1, // High-contrast big turn arrow + speed + distance for motorcycle
  VIEW_MEDIA_PLAYER = 2, // Smartphone Music Player remote deck (Prev/Play/Pause/Stop/Next/Vol)
};

enum NotifyKind : uint8_t {
  NOTIFY_NONE     = 0,
  NOTIFY_WHATSAPP = 1,
  NOTIFY_CALL     = 2,
  NOTIFY_INFO     = 3,
  NOTIFY_MEDIA    = 4,
};

struct NotifyState {
  bool active = false;
  NotifyKind kind = NOTIFY_NONE;
  char sender[48] = {};
  char message[96] = {};
  uint32_t shownAtMs = 0;
  uint32_t durationMs = 3800;
};

enum MediaPlaybackState : uint8_t {
  MEDIA_STOPPED = 0,
  MEDIA_PLAYING = 1,
  MEDIA_PAUSED  = 2,
};

enum MediaCommand : uint8_t {
  MEDIA_CMD_NONE       = 0,
  MEDIA_CMD_PREV       = 1,
  MEDIA_CMD_PLAY_PAUSE = 2,
  MEDIA_CMD_NEXT       = 3,
  MEDIA_CMD_STOP       = 4,
  MEDIA_CMD_VOL_DOWN   = 5,
  MEDIA_CMD_VOL_UP     = 6,
};

struct MediaState {
  bool valid = false;
  MediaPlaybackState state = MEDIA_STOPPED;
  char title[64] = {};
  char artist[48] = {};
  char app[24] = {};
  uint32_t lastUpdateMs = 0;
};

struct HwGpsData {
  bool fixValid = false;
  uint8_t satellites = 0;
  double lat = 0.0;
  double lon = 0.0;
  float speedKmh = 0.0f;
  float courseDeg = 0.0f;
  float altitudeM = 0.0f;
  uint32_t lastUpdateMs = 0;
};

struct NavState {
  bool valid = false;
  double lat = 0.0;
  double lon = 0.0;
  float bearing = 0.0f;
  float speedKmh = -1.0f;
  int distanceM = 0;
  int remainingM = -1;
  int remainingS = -1;
  int8_t phoneBattery = -1;
  bool offRoute = false;
  DisplayLang lang = LANG_ID;
  bool gpsWeak = false;
  char timeStr[8] = {};
  char instruction[128] = {};
  char street[96] = {};
  char html[NAV_HTML_MAX] = {};
  bool hasHtml = false;
  int routeCount = 0;
  int16_t routeX[NAV_MAX_ROUTE_POINTS] = {};
  int16_t routeY[NAV_MAX_ROUTE_POINTS] = {};
  int streetSegmentCount = 0;
  int16_t streetX0[NAV_MAX_STREET_SEGMENTS] = {};
  int16_t streetY0[NAV_MAX_STREET_SEGMENTS] = {};
  int16_t streetX1[NAV_MAX_STREET_SEGMENTS] = {};
  int16_t streetY1[NAV_MAX_STREET_SEGMENTS] = {};
  int16_t userX = 0;
  int16_t userY = 0;
  bool hasUserPosition = false;
  bool hasChronosIcon = false;
  uint8_t chronosIcon[288] = {};
};
