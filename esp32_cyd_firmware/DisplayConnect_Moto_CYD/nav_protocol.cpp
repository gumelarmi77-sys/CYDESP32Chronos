#include "nav_protocol.h"
#include <Arduino.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void copyTruncated(char* dest, size_t destSize, const char* src) {
  if (dest == nullptr || destSize == 0) return;
  if (src == nullptr) {
    dest[0] = '\0';
    return;
  }
  strncpy(dest, src, destSize - 1);
  dest[destSize - 1] = '\0';

  size_t valid = 0;
  for (size_t i = 0; dest[i];) {
    const uint8_t lead = static_cast<uint8_t>(dest[i]);
    const size_t bytes = lead < 128 ? 1 : (lead & 0xE0) == 0xC0 ? 2 : (lead & 0xF0) == 0xE0 ? 3 : (lead & 0xF8) == 0xF0 ? 4 : 1;
    bool complete = true;
    for (size_t n = 1; n < bytes; ++n) {
      if (i + n >= destSize || dest[i + n] == '\0' || (static_cast<uint8_t>(dest[i + n]) & 0xC0) != 0x80) {
        complete = false;
        break;
      }
    }
    if (!complete) break;
    i += bytes;
    valid = i;
  }
  dest[valid] = '\0';
}

// -----------------------------------------------------------------------------
// Zero-dependency lightweight JSON parser (no ArduinoJson library required)
// -----------------------------------------------------------------------------

static size_t skipWs(const char* s, size_t pos, size_t len) {
  while (pos < len && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r' || s[pos] == '\n')) {
    pos++;
  }
  return pos;
}

static size_t skipJsonString(const char* s, size_t pos, size_t len) {
  if (pos >= len || s[pos] != '"') return pos;
  pos++; // opening quote
  while (pos < len) {
    if (s[pos] == '\\') {
      pos += 2;
      continue;
    }
    if (s[pos] == '"') {
      return pos + 1;
    }
    pos++;
  }
  return len;
}

static size_t skipJsonValue(const char* s, size_t pos, size_t len) {
  pos = skipWs(s, pos, len);
  if (pos >= len) return len;
  if (s[pos] == '"') return skipJsonString(s, pos, len);
  if (s[pos] == '{' || s[pos] == '[') {
    int depth = 0;
    while (pos < len) {
      if (s[pos] == '"') {
        pos = skipJsonString(s, pos, len);
        continue;
      }
      if (s[pos] == '{' || s[pos] == '[') depth++;
      else if (s[pos] == '}' || s[pos] == ']') {
        depth--;
        pos++;
        if (depth <= 0) return pos;
        continue;
      }
      pos++;
    }
    return len;
  }
  while (pos < len && s[pos] != ',' && s[pos] != '}' && s[pos] != ']' &&
         s[pos] != ' ' && s[pos] != '\t' && s[pos] != '\r' && s[pos] != '\n') {
    pos++;
  }
  return pos;
}

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

static void decodeJsonString(const char* s, size_t start, size_t end, char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (end <= start + 1 || s[start] != '"') return;

  char tmp[512];
  size_t w = 0;
  size_t i = start + 1;
  const size_t limit = (end > 0 && s[end - 1] == '"') ? end - 1 : end;

  while (i < limit && w + 4 < sizeof(tmp)) {
    if (s[i] == '\\' && i + 1 < limit) {
      char esc = s[i + 1];
      if (esc == '"' || esc == '\\' || esc == '/') {
        tmp[w++] = esc;
        i += 2;
      } else if (esc == 'n') {
        tmp[w++] = '\n';
        i += 2;
      } else if (esc == 'r') {
        tmp[w++] = '\r';
        i += 2;
      } else if (esc == 't') {
        tmp[w++] = '\t';
        i += 2;
      } else if (esc == 'u' && i + 5 < limit) {
        int h0 = hexVal(s[i + 2]);
        int h1 = hexVal(s[i + 3]);
        int h2 = hexVal(s[i + 4]);
        int h3 = hexVal(s[i + 5]);
        if (h0 >= 0 && h1 >= 0 && h2 >= 0 && h3 >= 0) {
          uint16_t cp = static_cast<uint16_t>((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
          if (cp < 0x80) {
            tmp[w++] = static_cast<char>(cp);
          } else if (cp < 0x800) {
            tmp[w++] = static_cast<char>(0xC0 | (cp >> 6));
            tmp[w++] = static_cast<char>(0x80 | (cp & 0x3F));
          } else {
            tmp[w++] = static_cast<char>(0xE0 | (cp >> 12));
            tmp[w++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            tmp[w++] = static_cast<char>(0x80 | (cp & 0x3F));
          }
          i += 6;
        } else {
          tmp[w++] = s[i++];
        }
      } else {
        tmp[w++] = esc;
        i += 2;
      }
    } else {
      tmp[w++] = s[i++];
    }
  }
  tmp[w] = '\0';
  copyTruncated(out, outSize, tmp);
}

static bool findTopLevelKey(const char* json, size_t len, const char* targetKey,
                            size_t& valStart, size_t& valEnd) {
  if (!json || len < 2) return false;
  size_t pos = skipWs(json, 0, len);
  if (pos >= len || json[pos] != '{') return false;
  pos++;

  char keyBuf[64];
  while (pos < len) {
    pos = skipWs(json, pos, len);
    if (pos >= len || json[pos] == '}') return false;
    if (json[pos] == ',') {
      pos++;
      continue;
    }
    if (json[pos] != '"') return false;

    const size_t keyStart = pos;
    const size_t keyEnd = skipJsonString(json, pos, len);
    decodeJsonString(json, keyStart, keyEnd, keyBuf, sizeof(keyBuf));
    pos = skipWs(json, keyEnd, len);
    if (pos >= len || json[pos] != ':') return false;
    pos = skipWs(json, pos + 1, len);

    const size_t vStart = pos;
    const size_t vEnd = skipJsonValue(json, vStart, len);
    if (strcmp(keyBuf, targetKey) == 0) {
      valStart = vStart;
      valEnd = vEnd;
      return true;
    }
    pos = vEnd;
  }
  return false;
}

static bool jsonGetString(const char* json, size_t len, const char* key, char* out, size_t outSize) {
  size_t vs = 0, ve = 0;
  if (!findTopLevelKey(json, len, key, vs, ve)) return false;
  if (vs >= ve || json[vs] != '"') return false;
  decodeJsonString(json, vs, ve, out, outSize);
  return true;
}

static bool jsonGetDouble(const char* json, size_t len, const char* key, double& outVal) {
  size_t vs = 0, ve = 0;
  if (!findTopLevelKey(json, len, key, vs, ve)) return false;
  if (vs >= ve || json[vs] == '"' || json[vs] == 'n') return false;
  char buf[40];
  size_t copyLen = ve - vs;
  if (copyLen >= sizeof(buf)) copyLen = sizeof(buf) - 1;
  memcpy(buf, json + vs, copyLen);
  buf[copyLen] = '\0';
  char* endPtr = nullptr;
  double parsed = strtod(buf, &endPtr);
  if (endPtr == buf) return false;
  outVal = parsed;
  return true;
}

static int jsonGetInt(const char* json, size_t len, const char* key, int defaultVal, bool* found = nullptr) {
  double d = 0.0;
  if (!jsonGetDouble(json, len, key, d)) {
    if (found) *found = false;
    return defaultVal;
  }
  if (found) *found = true;
  return static_cast<int>(d);
}

static bool jsonGetBool(const char* json, size_t len, const char* key, bool defaultVal) {
  size_t vs = 0, ve = 0;
  if (!findTopLevelKey(json, len, key, vs, ve)) return defaultVal;
  if (ve - vs >= 4 && strncmp(json + vs, "true", 4) == 0) return true;
  if (ve - vs >= 5 && strncmp(json + vs, "false", 5) == 0) return false;
  return defaultVal;
}

// -----------------------------------------------------------------------------
// Protocol Public Functions
// -----------------------------------------------------------------------------

DisplayLang parseLangString(const char* langStr) {
  if (!langStr) return LANG_ID;
  if (strncmp(langStr, "en", 2) == 0) return LANG_EN;
  if (strncmp(langStr, "pt", 2) == 0) return LANG_PT;
  return LANG_ID;
}

bool isLoadingJson(const char* json, size_t length) {
  if (json == nullptr || length < 10) return false;
  char typeBuf[24] = {};
  if (jsonGetString(json, length, "type", typeBuf, sizeof(typeBuf))) {
    return strcmp(typeBuf, "loading") == 0;
  }
  return strstr(json, "\"type\":\"loading\"") != nullptr ||
         strstr(json, "\"type\": \"loading\"") != nullptr;
}

bool parseConfigLangJson(const char* json, size_t length, DisplayLang& outLang) {
  char typeBuf[24] = {};
  if (!jsonGetString(json, length, "type", typeBuf, sizeof(typeBuf)) ||
      strcmp(typeBuf, "config") != 0) {
    return false;
  }
  char langBuf[16] = "id";
  jsonGetString(json, length, "lang", langBuf, sizeof(langBuf));
  outLang = parseLangString(langBuf);
  return true;
}

bool parseNavJson(const char* json, size_t length, NavState& state) {
  if (json == nullptr || length == 0) return false;

  char typeBuf[24] = {};
  if (!jsonGetString(json, length, "type", typeBuf, sizeof(typeBuf))) {
    return false;
  }
  if (strcmp(typeBuf, "heartbeat") == 0 || strcmp(typeBuf, "loading") == 0) {
    return false;
  }
  if (strcmp(typeBuf, "nav") != 0) {
    return false;
  }

  state.valid = true;
  double latVal = 0.0, lonVal = 0.0, brgVal = 0.0, spdVal = -1.0;
  jsonGetDouble(json, length, "lat", latVal);
  jsonGetDouble(json, length, "lon", lonVal);
  jsonGetDouble(json, length, "bearing", brgVal);
  jsonGetDouble(json, length, "speed_kmh", spdVal);

  state.lat = latVal;
  state.lon = lonVal;
  state.bearing = static_cast<float>(brgVal);
  state.speedKmh = static_cast<float>(spdVal);
  state.distanceM = jsonGetInt(json, length, "distance_m", 0);
  state.remainingM = jsonGetInt(json, length, "remaining_m", -1);
  state.remainingS = jsonGetInt(json, length, "remaining_s", -1);
  int batt = jsonGetInt(json, length, "batt", -1);
  if (batt < 0) {
    batt = jsonGetInt(json, length, "battery", -1);
  }
  state.phoneBattery = (batt >= 0 && batt <= 100) ? static_cast<int8_t>(batt) : -1;
  state.offRoute = jsonGetBool(json, length, "off_route", false);
  state.gpsWeak = jsonGetBool(json, length, "gps_weak", false);

  state.timeStr[0] = '\0';
  jsonGetString(json, length, "time", state.timeStr, sizeof(state.timeStr));

  char langBuf[16] = "id";
  jsonGetString(json, length, "lang", langBuf, sizeof(langBuf));
  state.lang = parseLangString(langBuf);

  state.instruction[0] = '\0';
  jsonGetString(json, length, "instruction", state.instruction, sizeof(state.instruction));

  state.street[0] = '\0';
  jsonGetString(json, length, "street", state.street, sizeof(state.street));

  state.html[0] = '\0';
  state.hasHtml = jsonGetString(json, length, "html", state.html, sizeof(state.html)) && (state.html[0] != '\0');

  state.routeCount = 0;
  state.streetSegmentCount = 0;
  state.hasUserPosition = false;

  // Parse "route": [[x,y], [x,y], ...]
  size_t rs = 0, re = 0;
  if (findTopLevelKey(json, length, "route", rs, re) && rs < re && json[rs] == '[') {
    size_t p = rs + 1;
    while (p < re && state.routeCount < NAV_MAX_ROUTE_POINTS) {
      p = skipWs(json, p, re);
      if (p >= re || json[p] == ']') break;
      if (json[p] == '[') {
        p++;
        int nums[2] = {0, 0};
        int cnt = 0;
        while (p < re && json[p] != ']') {
          p = skipWs(json, p, re);
          if (p < re && (json[p] == '-' || isdigit(static_cast<unsigned char>(json[p])))) {
            char* endPtr = nullptr;
            long v = strtol(json + p, &endPtr, 10);
            if (cnt < 2) nums[cnt] = static_cast<int>(v);
            cnt++;
            p = static_cast<size_t>(endPtr - json);
          } else {
            p++;
          }
        }
        if (cnt >= 2) {
          state.routeX[state.routeCount] = static_cast<int16_t>(nums[0]);
          state.routeY[state.routeCount] = static_cast<int16_t>(nums[1]);
          state.routeCount++;
        }
        if (p < re && json[p] == ']') p++;
      } else {
        p++;
      }
    }
  }

  // Parse "streets": [[x0,y0,x1,y1], ...]
  size_t ss = 0, se = 0;
  if (findTopLevelKey(json, length, "streets", ss, se) && ss < se && json[ss] == '[') {
    size_t p = ss + 1;
    while (p < se && state.streetSegmentCount < NAV_MAX_STREET_SEGMENTS) {
      p = skipWs(json, p, se);
      if (p >= se || json[p] == ']') break;
      if (json[p] == '[') {
        p++;
        int nums[4] = {0, 0, 0, 0};
        int cnt = 0;
        while (p < se && json[p] != ']') {
          p = skipWs(json, p, se);
          if (p < se && (json[p] == '-' || isdigit(static_cast<unsigned char>(json[p])))) {
            char* endPtr = nullptr;
            long v = strtol(json + p, &endPtr, 10);
            if (cnt < 4) nums[cnt] = static_cast<int>(v);
            cnt++;
            p = static_cast<size_t>(endPtr - json);
          } else {
            p++;
          }
        }
        if (cnt >= 4) {
          state.streetX0[state.streetSegmentCount] = static_cast<int16_t>(nums[0]);
          state.streetY0[state.streetSegmentCount] = static_cast<int16_t>(nums[1]);
          state.streetX1[state.streetSegmentCount] = static_cast<int16_t>(nums[2]);
          state.streetY1[state.streetSegmentCount] = static_cast<int16_t>(nums[3]);
          state.streetSegmentCount++;
        }
        if (p < se && json[p] == ']') p++;
      } else {
        p++;
      }
    }
  }

  bool hasUx = false, hasUy = false;
  int ux = jsonGetInt(json, length, "user_x", 0, &hasUx);
  int uy = jsonGetInt(json, length, "user_y", 0, &hasUy);
  if (hasUx && hasUy) {
    state.userX = static_cast<int16_t>(ux);
    state.userY = static_cast<int16_t>(uy);
    state.hasUserPosition = true;
  } else if (state.routeCount > 0) {
    state.userX = state.routeX[state.routeCount - 1];
    state.userY = state.routeY[state.routeCount - 1];
    state.hasUserPosition = (state.userX >= 0 && state.userY >= 0);
  }

  return true;
}

bool parseNotifyJson(const char* json, size_t length, NotifyState& notify) {
  if (json == nullptr || length == 0) return false;

  char typeBuf[24] = {};
  if (!jsonGetString(json, length, "type", typeBuf, sizeof(typeBuf)) ||
      strcmp(typeBuf, "notify") != 0) {
    return false;
  }

  char kind[24] = "info";
  jsonGetString(json, length, "kind", kind, sizeof(kind));
  if (strcmp(kind, "clear") == 0) {
    notify.active = false;
    notify.kind = NOTIFY_NONE;
    notify.sender[0] = '\0';
    notify.message[0] = '\0';
    return true;
  }

  notify.active = true;
  notify.shownAtMs = millis();

  if (strcmp(kind, "whatsapp") == 0 || strcmp(kind, "wa") == 0) {
    notify.kind = NOTIFY_WHATSAPP;
    notify.durationMs = 3800;
  } else if (strcmp(kind, "call") == 0) {
    notify.kind = NOTIFY_CALL;
    notify.durationMs = 6000;
  } else if (strcmp(kind, "media") == 0) {
    notify.kind = NOTIFY_MEDIA;
    notify.durationMs = 3500;
  } else {
    notify.kind = NOTIFY_INFO;
    notify.durationMs = 3500;
  }

  copyTruncated(notify.sender, sizeof(notify.sender), "Notifikasi");
  jsonGetString(json, length, "sender", notify.sender, sizeof(notify.sender));

  notify.message[0] = '\0';
  jsonGetString(json, length, "message", notify.message, sizeof(notify.message));
  return true;
}

bool parseMediaJson(const char* json, size_t length, MediaState& media) {
  if (json == nullptr || length == 0) return false;

  char typeBuf[24] = {};
  if (!jsonGetString(json, length, "type", typeBuf, sizeof(typeBuf)) ||
      strcmp(typeBuf, "media") != 0) {
    return false;
  }

  media.valid = true;
  media.lastUpdateMs = millis();

  char stateStr[24] = "stopped";
  jsonGetString(json, length, "state", stateStr, sizeof(stateStr));
  if (strcmp(stateStr, "playing") == 0) {
    media.state = MEDIA_PLAYING;
  } else if (strcmp(stateStr, "paused") == 0) {
    media.state = MEDIA_PAUSED;
  } else {
    media.state = MEDIA_STOPPED;
  }

  copyTruncated(media.title, sizeof(media.title), "Tidak ada audio");
  jsonGetString(json, length, "title", media.title, sizeof(media.title));

  copyTruncated(media.artist, sizeof(media.artist), "Smartphone Media");
  jsonGetString(json, length, "artist", media.artist, sizeof(media.artist));

  copyTruncated(media.app, sizeof(media.app), "AUDIO");
  jsonGetString(json, length, "app", media.app, sizeof(media.app));
  return true;
}
