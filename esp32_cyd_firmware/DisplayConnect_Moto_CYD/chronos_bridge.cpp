#include "chronos_bridge.h"

#if ENABLE_CHRONOS_APP

#include <Arduino.h>
#include <ChronosESP32.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static ChronosESP32 watch(BLE_DEVICE_NAME);
static ChronosBridgeOutput s_out = {};
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static volatile bool s_connPending   = false;
static volatile bool s_connValue     = false;
static volatile bool s_navPending    = false;
static volatile bool s_iconPending   = false;
static volatile bool s_timePending   = false;
static volatile bool s_notifyPending = false;
static volatile bool s_ringerPending = false;

static bool s_ringerActive = false;
static char s_pendingNotifySender[48]  = {};
static char s_pendingNotifyMessage[96] = {};
static NotifyKind s_pendingNotifyKind  = NOTIFY_NONE;
static char s_pendingCaller[48]        = {};

static uint32_t s_lastClockSyncMs = 0;

static bool strContainsCI(const char* hay, const char* needle) {
  if (!hay || !needle) return false;
  const size_t nLen = strlen(needle);
  if (nLen == 0) return true;
  for (const char* p = hay; *p; ++p) {
    size_t i = 0;
    while (i < nLen && p[i] &&
           tolower(static_cast<unsigned char>(p[i])) == tolower(static_cast<unsigned char>(needle[i]))) {
      i++;
    }
    if (i == nLen) return true;
  }
  return false;
}

static void copySafeUtf8(char* dst, size_t dstSize, const char* src) {
  if (!dst || dstSize == 0) return;
  if (!src) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';

  size_t valid = 0;
  for (size_t i = 0; dst[i];) {
    const uint8_t lead = static_cast<uint8_t>(dst[i]);
    const size_t bytes = lead < 128 ? 1 : (lead & 0xE0) == 0xC0 ? 2 : (lead & 0xF0) == 0xE0 ? 3 : (lead & 0xF8) == 0xF0 ? 4 : 1;
    bool complete = true;
    for (size_t n = 1; n < bytes; ++n) {
      if (i + n >= dstSize || dst[i + n] == '\0' || (static_cast<uint8_t>(dst[i + n]) & 0xC0) != 0x80) {
        complete = false;
        break;
      }
    }
    if (!complete) break;
    i += bytes;
    valid = i;
  }
  dst[valid] = '\0';
}

// Parse strings like "120 m", "1.2 km", "350m", "0.5 mi", "200 yd" into meters
static int parseChronosDistanceMeters(const String& s) {
  if (s.length() == 0) return 0;
  char buf[32];
  copySafeUtf8(buf, sizeof(buf), s.c_str());

  // Normalize comma decimal (e.g., "1,2 km" -> "1.2 km")
  for (size_t i = 0; buf[i]; i++) {
    if (buf[i] == ',') buf[i] = '.';
  }

  char* endPtr = nullptr;
  float val = strtof(buf, &endPtr);
  if (endPtr == buf || val < 0.0f) return 0;

  if (strContainsCI(buf, "km")) {
    return static_cast<int>(val * 1000.0f + 0.5f);
  }
  if (strContainsCI(buf, "mi")) {
    return static_cast<int>(val * 1609.34f + 0.5f);
  }
  if (strContainsCI(buf, "yd")) {
    return static_cast<int>(val * 0.9144f + 0.5f);
  }
  if (strContainsCI(buf, "ft")) {
    return static_cast<int>(val * 0.3048f + 0.5f);
  }
  return static_cast<int>(val + 0.5f);
}

// Parse strings like "12 min", "1 hr 25 min", "45 sec", "1 h 5 m" into seconds
static int parseChronosDurationSeconds(const String& s) {
  if (s.length() == 0) return -1;
  const char* p = s.c_str();
  int totalSec = 0;
  bool foundAny = false;

  while (*p) {
    if (isdigit(static_cast<unsigned char>(*p))) {
      char* endPtr = nullptr;
      long n = strtol(p, &endPtr, 10);
      p = endPtr;
      while (*p == ' ') p++;
      if (*p == 'h' || *p == 'H' || *p == 'j' || *p == 'J') {
        totalSec += static_cast<int>(n * 3600L);
      } else if (*p == 's' || *p == 'S' || *p == 'd' || *p == 'D') {
        totalSec += static_cast<int>(n);
      } else {
        totalSec += static_cast<int>(n * 60L);
      }
      foundAny = true;
    } else {
      p++;
    }
  }
  return foundAny ? totalSec : -1;
}

static float parseChronosSpeedKmh(const String& s) {
  if (s.length() == 0) return -1.0f;
  char* endPtr = nullptr;
  float v = strtof(s.c_str(), &endPtr);
  if (endPtr == s.c_str() || v < 0.0f) return -1.0f;
  if (strContainsCI(s.c_str(), "mph")) {
    v *= 1.60934f;
  }
  return v;
}

static void onChronosConnection(bool state) {
  portENTER_CRITICAL(&s_mux);
  s_connValue   = state;
  s_connPending = true;
  portEXIT_CRITICAL(&s_mux);
}

static void onChronosNotification(Notification n) {
  const bool isWa = strContainsCI(n.app.c_str(), "whatsapp");
  const bool isCall = strContainsCI(n.title.c_str(), "panggilan") ||
                      strContainsCI(n.message.c_str(), "incoming call") ||
                      strContainsCI(n.message.c_str(), "panggilan suara") ||
                      strContainsCI(n.message.c_str(), "panggilan video");

  portENTER_CRITICAL(&s_mux);
  s_pendingNotifyKind = isCall ? NOTIFY_CALL : (isWa ? NOTIFY_WHATSAPP : NOTIFY_INFO);
  copySafeUtf8(s_pendingNotifySender, sizeof(s_pendingNotifySender),
               n.title.length() > 0 ? n.title.c_str() : n.app.c_str());
  copySafeUtf8(s_pendingNotifyMessage, sizeof(s_pendingNotifyMessage),
               n.message.c_str());
  s_notifyPending = true;
  portEXIT_CRITICAL(&s_mux);
}

static void onChronosRinger(String caller, bool state) {
  portENTER_CRITICAL(&s_mux);
  s_ringerActive = state;
  copySafeUtf8(s_pendingCaller, sizeof(s_pendingCaller), caller.c_str());
  s_ringerPending = true;
  portEXIT_CRITICAL(&s_mux);
}

static void onChronosConfig(Config config, uint32_t a, uint32_t b) {
  (void)a;
  (void)b;
  portENTER_CRITICAL(&s_mux);
  if (config == CF_NAV_DATA) {
    s_navPending = true;
  } else if (config == CF_NAV_ICON) {
    s_iconPending = true;
  } else if (config == CF_TIME) {
    s_timePending = true;
  }
  portEXIT_CRITICAL(&s_mux);
}

bool chronosBridgeInit(const ChronosBridgeOutput& out) {
  s_out = out;

  watch.setConnectionCallback(onChronosConnection);
  watch.setNotificationCallback(onChronosNotification);
  watch.setRingerCallback(onChronosRinger);
  watch.setConfigurationCallback(onChronosConfig);
  watch.begin();

  Serial.printf("ChronosESP32 BLE advertising as \"%s\"\n", BLE_DEVICE_NAME);
  return true;
}

void chronosBridgeLoop(uint32_t nowMs) {
  watch.loop();

  bool connChanged = false, connVal = false;
  bool navDirty = false, iconDirty = false, timeDirty = false;
  bool notifyDirty = false, ringerDirty = false, ringerState = false;
  NotifyKind nKind = NOTIFY_NONE;
  char nSender[48] = {};
  char nMsg[96] = {};
  char callerBuf[48] = {};

  portENTER_CRITICAL(&s_mux);
  if (s_connPending) {
    connChanged = true;
    connVal = s_connValue;
    s_connPending = false;
  }
  if (s_navPending) {
    navDirty = true;
    s_navPending = false;
  }
  if (s_iconPending) {
    iconDirty = true;
    s_iconPending = false;
  }
  if (s_timePending) {
    timeDirty = true;
    s_timePending = false;
  }
  if (s_notifyPending) {
    notifyDirty = true;
    nKind = s_pendingNotifyKind;
    memcpy(nSender, s_pendingNotifySender, sizeof(nSender));
    memcpy(nMsg, s_pendingNotifyMessage, sizeof(nMsg));
    s_notifyPending = false;
  }
  if (s_ringerPending) {
    ringerDirty = true;
    ringerState = s_ringerActive;
    memcpy(callerBuf, s_pendingCaller, sizeof(callerBuf));
    s_ringerPending = false;
  }
  portEXIT_CRITICAL(&s_mux);

  if (connChanged) {
    *s_out.clientConnected = connVal;
    if (connVal) {
      *s_out.awaitingFirstNav = true;
      *s_out.hasNav = false;
      *s_out.showingWaiting = false;
      *s_out.uiShowLoading = true;
    } else {
      *s_out.awaitingFirstNav = false;
      *s_out.hasNav = false;
      *s_out.uiShowWaiting = true;
    }
  }

  if (watch.isConnected() && (timeDirty || (nowMs - s_lastClockSyncMs >= 1000))) {
    s_lastClockSyncMs = nowMs;
    snprintf(s_out.lastNav->timeStr, sizeof(s_out.lastNav->timeStr), "%02d:%02d",
             watch.getHourC(), watch.getMinute());
    const int batt = static_cast<int>(watch.getPhoneBattery());
    if (batt >= 0 && batt <= 100) {
      s_out.lastNav->phoneBattery = static_cast<int8_t>(batt);
    }
  }

  if (navDirty || iconDirty) {
    Navigation& cNav = watch.getNavigation();
    if (!cNav.active) {
      if (*s_out.hasNav) {
        *s_out.hasNav = false;
        *s_out.awaitingFirstNav = false;
        if (s_out.onNavStopped) s_out.onNavStopped();
      }
    } else {
      const bool firstPacket = !(*s_out.hasNav);
      s_out.lastNav->valid = true;
      s_out.lastNav->offRoute = false;
      s_out.lastNav->gpsWeak = false;
      s_out.lastNav->routeCount = 0;
      s_out.lastNav->streetSegmentCount = 0;
      s_out.lastNav->hasUserPosition = false;
      s_out.lastNav->hasHtml = false;
      s_out.lastNav->html[0] = '\0';

      copySafeUtf8(s_out.lastNav->instruction, sizeof(s_out.lastNav->instruction),
                   cNav.directions.length() > 0 ? cNav.directions.c_str() : cNav.title.c_str());
      copySafeUtf8(s_out.lastNav->street, sizeof(s_out.lastNav->street),
                   cNav.title.c_str());

      s_out.lastNav->distanceM  = parseChronosDistanceMeters(cNav.distance);
      s_out.lastNav->remainingS = parseChronosDurationSeconds(cNav.duration);
      s_out.lastNav->remainingM = -1;

      const float spd = parseChronosSpeedKmh(cNav.speed);
      if (spd >= 0.0f) {
        s_out.lastNav->speedKmh = spd;
      } else if (s_out.hwGps != nullptr && s_out.hwGps->fixValid) {
        s_out.lastNav->speedKmh = s_out.hwGps->speedKmh;
      } else {
        s_out.lastNav->speedKmh = -1.0f;
      }

      if (s_out.hwGps != nullptr && s_out.hwGps->fixValid) {
        s_out.lastNav->lat = s_out.hwGps->lat;
        s_out.lastNav->lon = s_out.hwGps->lon;
        s_out.lastNav->bearing = s_out.hwGps->courseDeg;
      }

      s_out.lastNav->phoneBattery = static_cast<int8_t>(watch.getPhoneBattery());
      snprintf(s_out.lastNav->timeStr, sizeof(s_out.lastNav->timeStr), "%02d:%02d",
               watch.getHourC(), watch.getMinute());

      if (cNav.hasIcon) {
        memcpy(s_out.lastNav->chronosIcon, cNav.icon, sizeof(s_out.lastNav->chronosIcon));
        s_out.lastNav->hasChronosIcon = true;
      } else if (firstPacket) {
        s_out.lastNav->hasChronosIcon = false;
      }

      *s_out.hasNav = true;
      *s_out.showingWaiting = false;
      *s_out.awaitingFirstNav = false;

      if (s_out.onNavUpdated) {
        s_out.onNavUpdated(firstPacket);
      }
    }
  }

  if (notifyDirty) {
    s_out.activeNotify->active = true;
    s_out.activeNotify->kind = nKind;
    s_out.activeNotify->shownAtMs = nowMs;
    s_out.activeNotify->durationMs = (nKind == NOTIFY_CALL) ? 6000 : 3800;
    copySafeUtf8(s_out.activeNotify->sender, sizeof(s_out.activeNotify->sender), nSender);
    copySafeUtf8(s_out.activeNotify->message, sizeof(s_out.activeNotify->message), nMsg);
    if (s_out.onNotifyUpdated) {
      s_out.onNotifyUpdated();
    }
  }

  if (ringerDirty) {
    if (ringerState) {
      s_out.activeNotify->active = true;
      s_out.activeNotify->kind = NOTIFY_CALL;
      s_out.activeNotify->shownAtMs = nowMs;
      s_out.activeNotify->durationMs = 6000;
      copySafeUtf8(s_out.activeNotify->sender, sizeof(s_out.activeNotify->sender),
                   callerBuf[0] ? callerBuf : "Panggilan Masuk");
      copySafeUtf8(s_out.activeNotify->message, sizeof(s_out.activeNotify->message),
                   "Panggilan Telepon Masuk");
    } else if (s_out.activeNotify->kind == NOTIFY_CALL) {
      s_out.activeNotify->active = false;
      s_out.activeNotify->kind = NOTIFY_NONE;
    }
    if (s_out.onNotifyUpdated) {
      s_out.onNotifyUpdated();
    }
  }
}

void chronosBridgeSendMediaCmd(MediaCommand cmd) {
  if (!watch.isConnected()) return;

  switch (cmd) {
    case MEDIA_CMD_PREV:
      watch.musicControl(MUSIC_PREVIOUS);
      break;
    case MEDIA_CMD_PLAY_PAUSE:
      watch.musicControl(MUSIC_TOGGLE);
      break;
    case MEDIA_CMD_NEXT:
      watch.musicControl(MUSIC_NEXT);
      break;
    case MEDIA_CMD_STOP:
      watch.musicControl(MUSIC_PAUSE);
      break;
    case MEDIA_CMD_VOL_DOWN:
      watch.musicControl(VOLUME_DOWN);
      break;
    case MEDIA_CMD_VOL_UP:
      watch.musicControl(VOLUME_UP);
      break;
    default:
      break;
  }
}

void chronosBridgeDisconnect() {
  // Handled internally by ChronosESP32 stack
}

#endif
