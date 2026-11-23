#include "maps_theme.h"
#include "config.h"
#include <Arduino.h>
#include <Preferences.h>
#include <math.h>

static Preferences s_prefs;
static MapsTheme s_theme = MAPS_THEME_DARK;
static DisplayViewMode s_viewMode = VIEW_MAP_VECTOR;
static uint8_t s_briLevel = 2; // 0=25%, 1=65%, 2=100%

// LDR Auto-Theme state (GPIO 34 on ESP32-2432S028R: lower ADC = brighter, higher ADC = darker)
static bool s_autoLdrEnabled = true;
static uint32_t s_lastLdrPollMs = 0;
static uint16_t s_ldrEma = 1500;
static uint16_t s_lastRawAdc = 1500;
static uint8_t s_ldrConfirmCount = 0;
static MapsTheme s_ldrCandidate = MAPS_THEME_DARK;

static const uint16_t LDR_DAY_THRESHOLD   = 950;   // Bright sunlight / strong daylight
static const uint16_t LDR_NIGHT_THRESHOLD = 2150;  // Dusk / tunnel / night riding
static const uint32_t LDR_POLL_INTERVAL   = 250;   // ms for responsive EMA
static const uint8_t  LDR_CONFIRM_SAMPLES = 5;     // ~1.25s debounce against streetlights

// Perceptual Gamma 2.2 PWM backlight fade state
static float s_ldrBriEma = 1500.0f;
static uint32_t s_lastBriSampleMs = 0;
static float s_curPwm = 255.0f;
static uint8_t s_lastAppliedDuty = 255;

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void mapsThemeInit() {
  if (s_prefs.begin("ui", true)) {
    const uint8_t v = s_prefs.getUChar("theme", MAPS_THEME_DARK);
    if (v <= MAPS_THEME_DARK) s_theme = static_cast<MapsTheme>(v);

    const uint8_t m = s_prefs.getUChar("view", VIEW_MAP_VECTOR);
    if (m <= VIEW_MEDIA_PLAYER) s_viewMode = static_cast<DisplayViewMode>(m);

    const uint8_t b = s_prefs.getUChar("bri", 2);
    if (b <= 2) s_briLevel = b;

    s_autoLdrEnabled = s_prefs.getBool("autoldr", true);
    s_prefs.end();
  }

#ifdef LDR_PIN
  pinMode(LDR_PIN, INPUT);
  s_lastRawAdc = static_cast<uint16_t>(analogRead(LDR_PIN));
  s_ldrEma = s_lastRawAdc;
  s_ldrBriEma = static_cast<float>(s_lastRawAdc);
  if (s_autoLdrEnabled) {
    if (s_ldrEma < LDR_DAY_THRESHOLD) {
      s_theme = MAPS_THEME_LIGHT;
    } else if (s_ldrEma > LDR_NIGHT_THRESHOLD) {
      s_theme = MAPS_THEME_DARK;
    }
  }
#endif
  s_curPwm = static_cast<float>(backlightPwmDuty());
  s_lastAppliedDuty = static_cast<uint8_t>(s_curPwm);
}

MapsTheme mapsThemeGet() { return s_theme; }
bool mapsThemeIsDark() { return s_theme == MAPS_THEME_DARK; }

void mapsThemeSet(MapsTheme theme) {
  s_theme = theme;
  if (s_prefs.begin("ui", false)) {
    s_prefs.putUChar("theme", static_cast<uint8_t>(theme));
    s_prefs.end();
  }
}

bool mapsThemeToggle() {
  // Manual touch override switches immediately and disables auto-LDR for deterministic rider control
  s_autoLdrEnabled = false;
  if (s_prefs.begin("ui", false)) {
    s_prefs.putBool("autoldr", false);
    s_prefs.end();
  }
  mapsThemeSet(s_theme == MAPS_THEME_DARK ? MAPS_THEME_LIGHT : MAPS_THEME_DARK);
  return mapsThemeIsDark();
}

bool mapsThemeAutoGet() { return s_autoLdrEnabled; }

void mapsThemeAutoSet(bool enabled) {
  s_autoLdrEnabled = enabled;
  s_ldrConfirmCount = 0;
  if (s_prefs.begin("ui", false)) {
    s_prefs.putBool("autoldr", enabled);
    s_prefs.end();
  }
}

uint16_t mapsThemeLastLdrAdc() { return s_lastRawAdc; }

bool mapsThemePollLdr(uint32_t nowMs) {
#ifdef LDR_PIN
  if (!s_autoLdrEnabled) return false;
  if (nowMs - s_lastLdrPollMs < LDR_POLL_INTERVAL) return false;
  s_lastLdrPollMs = nowMs;

  const uint16_t sample = static_cast<uint16_t>(analogRead(LDR_PIN));
  s_lastRawAdc = sample;

  // Exponential Moving Average (EMA: 25% new, 75% previous)
  s_ldrEma = static_cast<uint16_t>((static_cast<uint32_t>(s_ldrEma) * 3U + sample) / 4U);

  MapsTheme target = s_theme;
  if (s_ldrEma <= LDR_DAY_THRESHOLD) {
    target = MAPS_THEME_LIGHT;
  } else if (s_ldrEma >= LDR_NIGHT_THRESHOLD) {
    target = MAPS_THEME_DARK;
  }

  if (target == s_theme) {
    s_ldrConfirmCount = 0;
    return false;
  }

  if (target != s_ldrCandidate) {
    s_ldrCandidate = target;
    s_ldrConfirmCount = 1;
    return false;
  }

  if (++s_ldrConfirmCount >= LDR_CONFIRM_SAMPLES) {
    s_ldrConfirmCount = 0;
    mapsThemeSet(target);
    return true;
  }
#else
  (void)nowMs;
#endif
  return false;
}

DisplayViewMode motoViewModeGet() { return s_viewMode; }

void motoViewModeSet(DisplayViewMode mode) {
  s_viewMode = mode;
  if (s_prefs.begin("ui", false)) {
    s_prefs.putUChar("view", static_cast<uint8_t>(mode));
    s_prefs.end();
  }
}

DisplayViewMode motoViewModeToggle() {
  DisplayViewMode next = VIEW_MAP_VECTOR;
  if (s_viewMode == VIEW_MAP_VECTOR) {
    next = VIEW_MOTO_HUD;
  } else if (s_viewMode == VIEW_MOTO_HUD) {
    next = VIEW_MEDIA_PLAYER;
  } else {
    next = VIEW_MAP_VECTOR;
  }
  motoViewModeSet(next);
  return s_viewMode;
}

uint8_t backlightLevelGet() { return s_briLevel; }

uint8_t backlightCycleLevel() {
  s_briLevel = (s_briLevel + 1) % 3;
  if (s_prefs.begin("ui", false)) {
    s_prefs.putUChar("bri", s_briLevel);
    s_prefs.end();
  }
  return s_briLevel;
}

uint8_t backlightPwmDuty() {
  switch (s_briLevel) {
    case 0: return 68;   // 25% Night Ride
    case 1: return 170;  // 65% Overcast / Dusk
    default: return 255; // 100% Direct Sunlight
  }
}

uint8_t backlightCurrentDuty() {
  return s_lastAppliedDuty;
}

uint8_t backlightTickFade(float dt, uint32_t nowMs, bool& outChanged) {
  outChanged = false;
  if (dt <= 0.0f || dt > 0.25f) dt = 0.016f;

  float targetDuty = static_cast<float>(backlightPwmDuty());

#ifdef LDR_PIN
  // Sample ambient light every 120ms with smooth EMA for perceptual auto-dimming
  if (nowMs - s_lastBriSampleMs >= 120) {
    s_lastBriSampleMs = nowMs;
    const uint16_t raw = static_cast<uint16_t>(analogRead(LDR_PIN));
    s_lastRawAdc = raw;
    s_ldrBriEma = s_ldrBriEma * 0.85f + static_cast<float>(raw) * 0.15f;
  }

  if (s_autoLdrEnabled) {
    // CYD LDR: ~250 = direct sun, ~3200 = pitch dark
    float norm = (2900.0f - s_ldrBriEma) / (2900.0f - 350.0f);
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;

    // Perceptual Gamma 2.2 curve scaled by user max-brightness cap (s_briLevel)
    const float gammaVal = powf(norm, 2.2f);
    const float userScale = static_cast<float>(backlightPwmDuty()) / 255.0f;
    targetDuty = (38.0f + 217.0f * gammaVal) * userScale;
    if (targetDuty < 32.0f) targetDuty = 32.0f;
    if (targetDuty > 255.0f) targetDuty = 255.0f;
  }
#else
  (void)nowMs;
#endif

  const float fadeAlpha = 1.0f - expf(-3.6f * dt);
  s_curPwm += (targetDuty - s_curPwm) * fadeAlpha;
  if (fabsf(targetDuty - s_curPwm) < 0.4f) {
    s_curPwm = targetDuty;
  }

  const uint8_t quantized = static_cast<uint8_t>(roundf(s_curPwm));
  if (quantized != s_lastAppliedDuty) {
    s_lastAppliedDuty = quantized;
    outChanged = true;
  }
  return s_lastAppliedDuty;
}

// =============================================================================
// Automotive Cartography Palette:
// - NIGHT MODE (DARK):  #12161F Dark Slate + Electric Blue (#2997FF) + Neon Orange (#FF7A1A)
// - DAY MODE   (LIGHT): #EBEFF4 Soft Map   + Google Blue (#1A73E8)   + Moto Orange (#EA580C)
// =============================================================================

uint16_t mapsColLand() {
  return mapsThemeIsDark() ? rgb565(18, 22, 31) : rgb565(235, 239, 244);
}

uint16_t mapsColPark() {
  return mapsThemeIsDark() ? rgb565(23, 30, 43) : rgb565(216, 232, 217);
}

uint16_t mapsColGrid() {
  return mapsThemeIsDark() ? rgb565(28, 36, 51) : rgb565(222, 228, 237);
}

uint16_t mapsColRoadEdge() {
  return mapsThemeIsDark() ? rgb565(35, 45, 64) : rgb565(184, 194, 209);
}

uint16_t mapsColRoadFill() {
  return mapsThemeIsDark() ? rgb565(56, 70, 94) : rgb565(255, 255, 255);
}

uint16_t mapsColRouteEdge() {
  return mapsThemeIsDark() ? rgb565(10, 44, 92) : rgb565(13, 71, 161);
}

uint16_t mapsColRoute() {
  return mapsThemeIsDark() ? rgb565(41, 151, 255) : rgb565(26, 115, 232);
}

uint16_t mapsColRouteDeep() {
  return mapsThemeIsDark() ? rgb565(255, 122, 26) : rgb565(234, 88, 12);
}

uint16_t mapsColCard() {
  return mapsThemeIsDark() ? rgb565(26, 33, 48) : rgb565(255, 255, 255);
}

uint16_t mapsColCardAlt() {
  return mapsThemeIsDark() ? rgb565(34, 43, 62) : rgb565(241, 245, 249);
}

uint16_t mapsColCardBorder() {
  return mapsThemeIsDark() ? rgb565(46, 59, 82) : rgb565(203, 213, 225);
}

uint16_t mapsColCardShadow() {
  return mapsThemeIsDark() ? rgb565(10, 13, 19) : rgb565(182, 192, 206);
}

uint16_t mapsColText() {
  return mapsThemeIsDark() ? rgb565(244, 247, 252) : rgb565(15, 23, 42);
}

uint16_t mapsColMuted() {
  return mapsThemeIsDark() ? rgb565(139, 155, 180) : rgb565(100, 116, 139);
}

uint16_t mapsColAccent() {
  // Neon Orange (Night) / High-contrast Moto Orange (Day)
  return mapsThemeIsDark() ? rgb565(255, 122, 26) : rgb565(234, 88, 12);
}

uint16_t mapsColMonoEye() {
  // Electric Blue (Night) / Google Nav Blue (Day)
  return mapsThemeIsDark() ? rgb565(41, 151, 255) : rgb565(26, 115, 232);
}

uint16_t mapsColWhite() {
  return rgb565(252, 253, 255);
}

uint16_t mapsColSwitchTrack() {
  return mapsThemeIsDark() ? rgb565(36, 46, 66) : rgb565(226, 232, 240);
}

uint16_t mapsColSwitchKnob() {
  return mapsThemeIsDark() ? rgb565(255, 122, 26) : rgb565(26, 115, 232);
}

uint16_t mapsColDanger() {
  return mapsThemeIsDark() ? rgb565(255, 69, 58) : rgb565(220, 38, 38);
}

uint16_t mapsColSuccess() {
  return mapsThemeIsDark() ? rgb565(48, 209, 88) : rgb565(22, 163, 74);
}
