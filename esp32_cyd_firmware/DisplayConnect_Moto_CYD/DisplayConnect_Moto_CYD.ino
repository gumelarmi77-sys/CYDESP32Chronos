/*
 * DisplayConnect Motorcycle CYD Firmware (ESP32-2432S028R)
 * Based on https://github.com/malaq88/DisplayConnect (BLE NUS v3.0)
 *
 * Features:
 * - 60+ FPS Frame-Rate Independent Physics Smoothing (Lerp, Shortest-Angle Wrap, Peak-Hold Decay)
 * - Partial-Region Strip Sprite Compositor + Tabular Digit Dirty-Checking (Zero Flicker, ~21.31 KB Sprite RAM)
 * - Catmull-Rom Spline Curved Roads/Routes + 24x24 Rotating 3D Chevron Sprite
 * - Cubic Ease-Out Sliding Turn Card & Bottom Toast Notifications
 * - LDR (GPIO 34) EMA + Gamma 2.2 Perceptual Auto-Dimming & Hysteresis Day/Night Switching
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <esp_system.h>
#include <esp_attr.h>

#include "config.h"
#include "nav_types.h"
#include "nav_protocol.h"
#include "map_renderer.h"
#include "maps_theme.h"
#include "touch_cyd.h"
#include "loading_screen.h"
#include "utf8_text.h"
#include "gps_hardware.h"
#if ENABLE_CHRONOS_APP
#include "chronos_bridge.h"
#else
#include <NimBLEDevice.h>
#endif

TFT_eSPI tft = TFT_eSPI();
MapRenderer mapRenderer(tft);

static volatile bool clientConnected  = false;
static volatile bool awaitingFirstNav = false;
static volatile bool uiShowLoading    = false;
static volatile bool uiShowWaiting    = false;
static volatile bool sendOkPending    = false;

static bool hasNav         = false;
static bool showingWaiting = false;
static NavState lastNav;
static NotifyState activeNotify;
static MediaState lastMedia;
static HwGpsData hwGps;

static uint32_t navUpdatesReceived = 0;
static uint32_t updatesPerSecond   = 0;
static uint32_t lastStatsMillis    = 0;
static uint32_t lastGpsRedrawMs    = 0;
static uint32_t lastFrameUs        = 0;
static uint32_t lastTickMs         = 0;

static bool touchWasDown          = false;
static uint32_t lastTouchActionMs = 0;

static int lastAlertedDistance = 99999;

// Persists across ESP_RST_PANIC (Reset reason 4) reboots in RTC memory
RTC_NOINIT_ATTR static uint32_t s_rtcBootMagic;
RTC_NOINIT_ATTR static uint32_t s_rtcBootStage;
static const uint32_t RTC_BOOT_MAGIC_VAL = 0xDC60F951UL;

// Line buffer for BLE newline-delimited JSON messages
static const size_t LINE_BUF_SIZE = 4096;
static char lineBuf[LINE_BUF_SIZE];
static size_t lineLen = 0;
static bool discardLine = false;

// Lock-free ring buffer filled in BLE onWrite callback, drained safely in loop()
static const size_t RX_QUEUE_SIZE = 8192;
static uint8_t rxQueue[RX_QUEUE_SIZE];
static volatile size_t rxHead = 0;
static volatile size_t rxTail = 0;
static volatile bool rxOverflow = false;

#if !ENABLE_CHRONOS_APP
static NimBLEServer* bleServer = nullptr;
static NimBLECharacteristic* txCharacteristic = nullptr;
#endif

// Forward declarations
bool initDisplay();
bool initBle();
void applyBacklightPwm(uint8_t duty);
void showStatusScreen(const char* title, const char* line2 = nullptr, const char* line3 = nullptr);
void showWaitingForAppScreen();
void handleUiFlags();
void pollTouchControls();
void redrawScreen();
void triggerTurnBuzzer(const NavState& state);
void sendMediaCommandBle(MediaCommand cmd);

#if ENABLE_CHRONOS_APP
static void onBridgeNavUpdated(bool firstPacket) {
  freeMapLoadingSprite();
  setDisplayLang(lastNav.lang);
  mapRenderer.setTargetNav(lastNav);
  if (firstPacket) {
    mapRenderer.draw(lastNav, hwGps, lastMedia, clientConnected);
  }
  triggerTurnBuzzer(lastNav);
  navUpdatesReceived++;
  updatesPerSecond++;
}

static void onBridgeNotifyUpdated() {
#if ENABLE_TURN_BUZZER
  if (activeNotify.active) {
    tone(BUZZER_PIN, activeNotify.kind == NOTIFY_CALL ? 2800 : 2100, 120);
  }
#endif
  if (activeNotify.active) {
    mapRenderer.triggerNotifyToast(activeNotify);
  } else {
    mapRenderer.dismissNotifyToast();
  }
  if (!hasNav || motoViewModeGet() != VIEW_MAP_VECTOR) {
    redrawScreen();
  }
}

static void onBridgeNavStopped() {
  mapRenderer.invalidateLayout();
  redrawScreen();
}
#else
void processTextMessage(const char* payload, size_t length);
void drainRxQueue();
void appendRxByte(char c);

static bool rxQueuePush(uint8_t b) {
  const size_t next = (rxHead + 1) % RX_QUEUE_SIZE;
  if (next == rxTail) {
    rxOverflow = true;
    return false;
  }
  rxQueue[rxHead] = b;
  rxHead = next;
  return true;
}

static bool rxQueuePop(uint8_t& b) {
  if (rxTail == rxHead) return false;
  b = rxQueue[rxTail];
  rxTail = (rxTail + 1) % RX_QUEUE_SIZE;
  return true;
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* /*server*/, NimBLEConnInfo& /*connInfo*/) override {
    clientConnected  = true;
    awaitingFirstNav = true;
    navUpdatesReceived = 0;
    updatesPerSecond = 0;
    lastStatsMillis  = millis();
    lineLen     = 0;
    discardLine = false;
    rxOverflow  = false;
    hasNav      = false;
    showingWaiting = false;
    uiShowLoading  = true;
    sendOkPending  = true;
  }

  void onDisconnect(NimBLEServer* /*server*/, NimBLEConnInfo& /*connInfo*/, int /*reason*/) override {
    clientConnected  = false;
    awaitingFirstNav = false;
    navUpdatesReceived = 0;
    lineLen     = 0;
    discardLine = false;
    rxOverflow  = false;
    hasNav      = false;
    rxHead      = 0;
    rxTail      = 0;
    uiShowWaiting = true;
  }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& /*connInfo*/) override {
    const NimBLEAttValue& value = characteristic->getValue();
    const size_t len = value.length();
    const uint8_t* data = value.data();
    for (size_t i = 0; i < len; i++) {
      if (!rxQueuePush(data[i])) break;
    }
  }
};
#endif

void setup() {
  Serial.begin(115200);
  delay(200);

  const esp_reset_reason_t rstReason = esp_reset_reason();
  const bool hadPanic = (rstReason == ESP_RST_PANIC) && (s_rtcBootMagic == RTC_BOOT_MAGIC_VAL);
  const uint32_t failedStage = hadPanic ? s_rtcBootStage : 0;

  s_rtcBootMagic = RTC_BOOT_MAGIC_VAL;
  s_rtcBootStage = 0;

  Serial.println(F("\n=== DisplayConnect Motorcycle CYD (ESP32 60FPS) ==="));
  Serial.printf("[BOOT] Reset reason: %d, Prev stage: %lu, Free heap: %u B\n",
                static_cast<int>(rstReason),
                static_cast<unsigned long>(failedStage),
                static_cast<unsigned>(ESP.getFreeHeap()));
  Serial.flush();

  mapsThemeInit();
  initDisplayLanguage();

  if (!initDisplay()) {
    Serial.println(F("TFT init failed!"));
    while (true) { delay(1000); }
  }
  Serial.println(F("[BOOT] TFT display initialized"));
  Serial.flush();

  auto showBootStep = [&](uint32_t stageId, const char* step) {
    s_rtcBootStage = stageId;
    tft.fillRect(16, 234, SCR_W - 32, 22, mapsColLand());
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(mapsColMuted(), mapsColLand());
    tft.drawString(step, SCR_W / 2, 245, 1);
    tft.setTextDatum(TL_DATUM);
  };

  if (hadPanic) {
    char panicMsg[48];
    snprintf(panicMsg, sizeof(panicMsg), "RECOVERY PANIC STAGE %lu", static_cast<unsigned long>(failedStage));
    Serial.println(panicMsg);
    Serial.flush();
    tft.fillScreen(mapsColLand());
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(mapsColDanger(), mapsColLand());
    tft.drawString(panicMsg, SCR_W / 2, 140, 2);
    tft.setTextDatum(TL_DATUM);
    delay(1800);
  }

  // Stage 1: Startup JDM gauge needle sweep (auto-skip if stage 1 panicked)
  if (!(hadPanic && failedStage == 1)) {
    s_rtcBootStage = 1;
    playBootSplashSweep(tft);
    Serial.printf("[BOOT] Splash sweep done, Free heap: %u B\n",
                  static_cast<unsigned>(ESP.getFreeHeap()));
    Serial.flush();
  } else {
    Serial.println(F("[BOOT] Skipping splash after Stage 1 panic"));
    Serial.flush();
  }

  // Stage 2: Allocate permanent partial-sprite pool (~21.31 KB total) once
  showBootStep(2, "INIT: SPRITES...");
  mapRenderer.begin();
  Serial.printf("[BOOT] MapRenderer sprites allocated, Free heap: %u B\n",
                static_cast<unsigned>(ESP.getFreeHeap()));
  Serial.flush();

  // Stage 3: Touch SPI init
  showBootStep(3, "INIT: TOUCH...");
  if (!touchInit()) {
    Serial.println(F("Touch init failed — touch controls disabled"));
  } else {
    Serial.println(F("[BOOT] Touch ready"));
  }
  Serial.flush();

#if ENABLE_TURN_BUZZER
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#endif

  // Stage 4: Hardware GPS UART2
  showBootStep(4, "INIT: GPS...");
  hwGpsInit();
  Serial.println(F("[BOOT] GPS init done, showing BLE status..."));
  Serial.flush();

  // Stage 5: Status UI
  showBootStep(5, "INIT: STATUS UI...");
  showStatusScreen("DisplayConnect Moto", "Starting BLE NUS...");
  Serial.println(F("[BOOT] Starting BLE..."));
  Serial.flush();

  // Stage 6: NimBLE Server init
  s_rtcBootStage = 6;
  if (!initBle()) {
    showStatusScreen("BLE Error", "Restart CYD Board");
    while (true) { delay(1000); }
  }
  Serial.printf("[BOOT] BLE started, Free heap: %u B\n",
                static_cast<unsigned>(ESP.getFreeHeap()));
  Serial.flush();

  // Stage 7: Standby UI & enter loop
  s_rtcBootStage = 7;
  showWaitingForAppScreen();
  showingWaiting = true;
  lastFrameUs = micros();
  lastTickMs = millis();
  s_rtcBootStage = 99;
  Serial.println(F("[BOOT] Ready -> Standby screen active"));
  Serial.flush();
}

void loop() {
  const uint32_t nowMs = millis();
  const uint32_t nowUs = micros();
  float dt = static_cast<float>(nowUs - lastFrameUs) * 1e-6f;
  if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;
  lastFrameUs = nowUs;

  handleUiFlags();
#if ENABLE_CHRONOS_APP
  chronosBridgeLoop(nowMs);
#else
  drainRxQueue();
#endif
  hwGpsPoll(hwGps);
  pollTouchControls();

  // 1. LDR Day/Night theme hysteresis + perceptual Gamma 2.2 PWM backlight fade
  if (mapsThemePollLdr(nowMs)) {
    mapRenderer.refreshThemeAssets();
    redrawScreen();
  }
  bool pwmChanged = false;
  const uint8_t smoothDuty = backlightTickFade(dt, nowMs, pwmChanged);
  if (pwmChanged) {
    applyBacklightPwm(smoothDuty);
  }

  // 2. Loading gauge animation (~60 FPS sprite sweep)
#if !ENABLE_CHRONOS_APP
  if (clientConnected && awaitingFirstNav) {
    updateMapLoadingAnimation(tft, nowMs);
  }
#endif

  // 3. Auto-dismiss transient toast notification with smooth slide-down
  if (activeNotify.active && (nowMs - activeNotify.shownAtMs >= activeNotify.durationMs)) {
    activeNotify.active = false;
    mapRenderer.dismissNotifyToast();
    if (!hasNav || motoViewModeGet() == VIEW_MEDIA_PLAYER) {
      redrawScreen();
    }
  }

  // 4. 60+ FPS Physics Interpolation & Partial Dirty-Rect Compositor
  if (hasNav && clientConnected && (nowMs - lastTickMs >= 16)) {
    const float frameDt = static_cast<float>(nowMs - lastTickMs) * 0.001f;
    lastTickMs = nowMs;
    mapRenderer.tick(frameDt, nowMs, hwGps, clientConnected, hasNav);
  }

  // 5. Standby Hardware GPS refresh
#if ENABLE_HW_GPS
  if (showingWaiting && motoViewModeGet() != VIEW_MEDIA_PLAYER && (nowMs - lastGpsRedrawMs >= 1000)) {
    lastGpsRedrawMs = nowMs;
    if (hwGps.fixValid || hwGps.satellites > 0) {
      mapRenderer.drawStandaloneGpsScreen(hwGps, clientConnected);
    }
  }
#endif

  delay(2);
}

void applyBacklightPwm(uint8_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(TFT_BL_PIN, duty);
#else
  ledcWrite(BL_PWM_CHANNEL, duty);
#endif
}

bool initDisplay() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(TFT_BL_PIN, BL_PWM_FREQ, BL_PWM_RES);
#else
  ledcSetup(BL_PWM_CHANNEL, BL_PWM_FREQ, BL_PWM_RES);
  ledcAttachPin(TFT_BL_PIN, BL_PWM_CHANNEL);
#endif
  applyBacklightPwm(backlightPwmDuty());

  tft.init();
  tft.writecommand(0x26);
  tft.writedata(2);
  delay(120);
  tft.writecommand(0x26);
  tft.writedata(1);
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(mapsColLand());
  return true;
}

bool initBle() {
#if ENABLE_CHRONOS_APP
  ChronosBridgeOutput out = {};
  out.clientConnected  = &clientConnected;
  out.awaitingFirstNav = &awaitingFirstNav;
  out.uiShowLoading    = &uiShowLoading;
  out.uiShowWaiting    = &uiShowWaiting;
  out.hasNav           = &hasNav;
  out.showingWaiting   = &showingWaiting;
  out.lastNav          = &lastNav;
  out.activeNotify     = &activeNotify;
  out.lastMedia        = &lastMedia;
  out.hwGps            = &hwGps;
  out.onNavUpdated     = onBridgeNavUpdated;
  out.onNotifyUpdated  = onBridgeNotifyUpdated;
  out.onNavStopped     = onBridgeNavStopped;
  return chronosBridgeInit(out);
#else
  NimBLEDevice::init(BLE_DEVICE_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);
  NimBLEDevice::setMTU(BLE_MTU);

  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());
  bleServer->advertiseOnDisconnect(false);

  NimBLEService* service = bleServer->createService(NUS_SERVICE_UUID);
  NimBLECharacteristic* rx = service->createCharacteristic(
    NUS_RX_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  rx->setCallbacks(new RxCallbacks());

  txCharacteristic = service->createCharacteristic(
    NUS_TX_UUID,
    NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ
  );

  service->start();
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(NUS_SERVICE_UUID);
  advertising->start();

  Serial.printf("BLE advertising as \"%s\"\n", BLE_DEVICE_NAME);
  return true;
#endif
}

void handleUiFlags() {
  if (uiShowLoading) {
    uiShowLoading = false;
#if ENABLE_CHRONOS_APP
    showingWaiting = true;
    awaitingFirstNav = false;
    mapRenderer.invalidateLayout();
    showStatusScreen("CHRONOS TERHUBUNG", "BLE: " BLE_DEVICE_NAME, "MULAI NAVIGASI GOOGLE MAPS");
#else
    showingWaiting = false;
    showMapLoadingScreen(tft);
#endif
  }
  if (uiShowWaiting) {
    uiShowWaiting = false;
    showingWaiting = true;
    freeMapLoadingSprite();
    mapRenderer.invalidateLayout();
    showWaitingForAppScreen();
#if !ENABLE_CHRONOS_APP
    if (bleServer != nullptr) {
      bleServer->startAdvertising();
    }
#endif
  }
#if !ENABLE_CHRONOS_APP
  if (sendOkPending && clientConnected && txCharacteristic != nullptr) {
    sendOkPending = false;
    txCharacteristic->setValue("OK\n");
    txCharacteristic->notify();
  }
#endif
}

void redrawScreen() {
  mapRenderer.invalidateLayout();
  if (motoViewModeGet() == VIEW_MEDIA_PLAYER) {
    mapRenderer.drawMediaPlayerMode(lastNav, hwGps, lastMedia, clientConnected);
    showingWaiting = !hasNav;
  } else if (hasNav && clientConnected) {
    mapRenderer.draw(lastNav, hwGps, lastMedia, clientConnected);
    showingWaiting = false;
  } else if (clientConnected && awaitingFirstNav) {
    showMapLoadingScreen(tft);
    showingWaiting = false;
  } else if (clientConnected) {
    showStatusScreen("CHRONOS TERHUBUNG", "BLE: " BLE_DEVICE_NAME, "MULAI NAVIGASI GOOGLE MAPS");
    showingWaiting = true;
  } else {
    showWaitingForAppScreen();
    showingWaiting = true;
  }
  if (activeNotify.active) {
    mapRenderer.drawNotifyBanner(activeNotify);
  }
}

void sendMediaCommandBle(MediaCommand cmd) {
  if (cmd == MEDIA_CMD_NONE) return;

  const char* cmdName = nullptr;
  switch (cmd) {
    case MEDIA_CMD_PREV:       cmdName = "prev"; break;
    case MEDIA_CMD_PLAY_PAUSE: cmdName = "play_pause"; break;
    case MEDIA_CMD_NEXT:       cmdName = "next"; break;
    case MEDIA_CMD_STOP:       cmdName = "stop"; break;
    case MEDIA_CMD_VOL_DOWN:   cmdName = "vol_down"; break;
    case MEDIA_CMD_VOL_UP:     cmdName = "vol_up"; break;
    default: break;
  }
  if (!cmdName) return;

  if (cmd == MEDIA_CMD_STOP) {
    lastMedia.valid = true;
    lastMedia.state = MEDIA_STOPPED;
  } else if (cmd == MEDIA_CMD_PLAY_PAUSE) {
    lastMedia.valid = true;
    lastMedia.state = (lastMedia.state == MEDIA_PLAYING) ? MEDIA_PAUSED : MEDIA_PLAYING;
  } else if (cmd == MEDIA_CMD_NEXT || cmd == MEDIA_CMD_PREV) {
    lastMedia.valid = true;
    lastMedia.state = MEDIA_PLAYING;
  }

#if ENABLE_CHRONOS_APP
  chronosBridgeSendMediaCmd(cmd);
#else
  if (clientConnected && txCharacteristic != nullptr) {
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"type\":\"media_cmd\",\"cmd\":\"%s\"}\n", cmdName);
    txCharacteristic->setValue(reinterpret_cast<const uint8_t*>(msg), strlen(msg));
    txCharacteristic->notify();
    Serial.printf("BLE TX media_cmd: %s\n", cmdName);
  }
#endif

#if ENABLE_TURN_BUZZER
  tone(BUZZER_PIN, 2200, 35);
#endif
  redrawScreen();
}

void pollTouchControls() {
  uint16_t x = 0, y = 0;
  const bool down = touchRead(x, y);
  const uint32_t now = millis();

  if (down && !touchWasDown && (now - lastTouchActionMs) > 300) {
    if (activeNotify.active && MapRenderer::notifyDismissHit(x, y)) {
      lastTouchActionMs = now;
      activeNotify.active = false;
      mapRenderer.dismissNotifyToast();
      if (!hasNav || motoViewModeGet() != VIEW_MAP_VECTOR) {
        redrawScreen();
      }
    } else if (MapRenderer::viewModeSwitchHit(x, y)) {
      lastTouchActionMs = now;
      motoViewModeToggle();
      redrawScreen();
    } else if (MapRenderer::brightnessSwitchHit(x, y)) {
      lastTouchActionMs = now;
      backlightCycleLevel();
      if (!hasNav || motoViewModeGet() == VIEW_MEDIA_PLAYER) {
        redrawScreen();
      } else {
        mapRenderer.drawTopNavButtons(clientConnected, &hwGps);
      }
    } else if (motoViewModeGet() == VIEW_MEDIA_PLAYER) {
      const MediaCommand cmd = MapRenderer::mediaControlHit(x, y);
      if (cmd != MEDIA_CMD_NONE) {
        lastTouchActionMs = now;
        sendMediaCommandBle(cmd);
      } else if (MapRenderer::themeSwitchHit(x, y)) {
        lastTouchActionMs = now;
        mapsThemeToggle();
        mapRenderer.refreshThemeAssets();
        redrawScreen();
      }
    } else if (MapRenderer::themeSwitchHit(x, y)) {
      lastTouchActionMs = now;
      mapsThemeToggle();
      mapRenderer.refreshThemeAssets();
      redrawScreen();
    }
  }
  touchWasDown = down;
}

void showStatusScreen(const char* title, const char* line2, const char* line3) {
  const uint16_t land = mapsColLand();
  const uint16_t card = mapsColCard();
  tft.fillScreen(land);

  for (int16_t gx = 40; gx < SCR_W; gx += 40) {
    tft.drawFastVLine(gx, 0, SCR_H, mapsColGrid());
  }
  for (int16_t gy = 40; gy < SCR_H; gy += 40) {
    tft.drawFastHLine(0, gy, SCR_W, mapsColGrid());
  }

  mapRenderer.drawTopNavButtons(clientConnected, &hwGps);

  tft.drawSmoothArc(SCR_W / 2, 62, 18, 15, 45, 315, mapsColMonoEye(), land, true);
  tft.drawSpot(SCR_W / 2, 62, 6.0f, mapsColAccent(), land);
  tft.drawSpot(SCR_W / 2, 62, 2.5f, mapsColWhite(), mapsColAccent());

  mapRenderer.drawFloatingCard(tft, 14, 94, SCR_W - 28, 122, 14, card, mapsColCardBorder(), land);

  tft.setTextDatum(MC_DATUM);
  if (title != nullptr) {
    tft.setTextColor(mapsColAccent(), card);
    tft.drawString(title, SCR_W / 2, 120, 2);
  }
  if (line2 != nullptr) {
    tft.setTextColor(mapsColText(), card);
    tft.drawString(line2, SCR_W / 2, 152, 2);
  }
  if (line3 != nullptr) {
    tft.setTextColor(mapsColMuted(), card);
    tft.drawString(line3, SCR_W / 2, 184, 1);
  }

  mapRenderer.drawThemeSwitchWaiting();
  tft.setTextColor(mapsColMuted(), land);
  tft.drawString(F("TAP KANAN ATAS: MAP / HUD / MUSIK"), SCR_W / 2, SCR_H - 12, 1);
  tft.setTextDatum(TL_DATUM);
}

void showWaitingForAppScreen() {
  const DisplayLang l = displayLangGet();
  const char* title = (l == LANG_ID) ? "SIAP NAVIGASI" : (l == LANG_EN ? "SYSTEM READY" : "PRONTO");
#if ENABLE_CHRONOS_APP
  showStatusScreen(title, "BLE: " BLE_DEVICE_NAME, "HUBUNGKAN APP CHRONOS");
#else
  showStatusScreen(title, "BLE: " BLE_DEVICE_NAME, "HUBUNGKAN APP DISPLAYCONNECT");
#endif
}

#if !ENABLE_CHRONOS_APP

void appendRxByte(char c) {
  if (c == '\n' || c == '\r') {
    if (lineLen > 0 && !discardLine) {
      lineBuf[lineLen] = '\0';
      processTextMessage(lineBuf, lineLen);
    }
    lineLen = 0;
    discardLine = false;
    return;
  }
  if (discardLine) return;
  if (lineLen + 1 >= LINE_BUF_SIZE) {
    lineLen = 0;
    discardLine = true;
    return;
  }
  lineBuf[lineLen++] = c;
}

void drainRxQueue() {
  if (rxOverflow) {
    rxTail = rxHead;
    rxOverflow = false;
    lineLen = 0;
    discardLine = true;
  }
  uint8_t b;
  for (int i = 0; i < 1024; i++) {
    if (!rxQueuePop(b)) break;
    appendRxByte(static_cast<char>(b));
  }
}

void triggerTurnBuzzer(const NavState& state) {
#if ENABLE_TURN_BUZZER
  if (state.offRoute) {
    tone(BUZZER_PIN, 1800, 120);
    return;
  }
  if (state.distanceM <= TURN_ALERT_DIST_M && lastAlertedDistance > TURN_ALERT_DIST_M) {
    tone(BUZZER_PIN, 2400, 80);
  }
  lastAlertedDistance = state.distanceM;
#else
  (void)state;
#endif
}

void processTextMessage(const char* payload, size_t length) {
  if (length < 150 && strstr(payload, "\"type\":\"config\"")) {
    DisplayLang newLang = LANG_ID;
    if (parseConfigLangJson(payload, length, newLang)) {
      setDisplayLang(newLang);
      redrawScreen();
    }
    return;
  }

  if (length < 512 && strstr(payload, "\"type\":\"notify\"")) {
    if (parseNotifyJson(payload, length, activeNotify)) {
#if ENABLE_TURN_BUZZER
      if (activeNotify.active) {
        tone(BUZZER_PIN, activeNotify.kind == NOTIFY_CALL ? 2800 : 2100, 120);
      }
#endif
      mapRenderer.triggerNotifyToast(activeNotify);
      if (!hasNav || motoViewModeGet() != VIEW_MAP_VECTOR) {
        redrawScreen();
      }
    }
    return;
  }

  if (length < 512 && strstr(payload, "\"type\":\"media\"")) {
    char prevTitle[64] = {};
    strncpy(prevTitle, lastMedia.title, sizeof(prevTitle) - 1);
    if (parseMediaJson(payload, length, lastMedia)) {
      if (motoViewModeGet() == VIEW_MEDIA_PLAYER) {
        redrawScreen();
      } else if (lastMedia.state == MEDIA_PLAYING &&
                 lastMedia.title[0] != '\0' &&
                 strcmp(prevTitle, lastMedia.title) != 0) {
        activeNotify.active = true;
        activeNotify.kind = NOTIFY_MEDIA;
        activeNotify.shownAtMs = millis();
        activeNotify.durationMs = 3500;
        snprintf(activeNotify.sender, sizeof(activeNotify.sender), "%s", lastMedia.title);
        snprintf(activeNotify.message, sizeof(activeNotify.message), "%s", lastMedia.artist);
        mapRenderer.triggerNotifyToast(activeNotify);
      }
    }
    return;
  }

  if (isLoadingJson(payload, length)) {
    awaitingFirstNav = true;
    hasNav = false;
    showingWaiting = false;
    mapRenderer.invalidateLayout();
    showMapLoadingScreen(tft);
    return;
  }

  NavState state;
  if (!parseNavJson(payload, length, state)) {
    return;
  }

  const bool firstPacket = !hasNav;
  freeMapLoadingSprite();
  setDisplayLang(state.lang);
  lastNav = state;
  hasNav = true;
  showingWaiting = false;
  awaitingFirstNav = false;

  // Push targets into 60 FPS physics engine (rendered smoothly by loop() tick)
  mapRenderer.setTargetNav(lastNav);
  if (firstPacket) {
    mapRenderer.draw(lastNav, hwGps, lastMedia, clientConnected);
  }
  triggerTurnBuzzer(lastNav);

  navUpdatesReceived++;
  updatesPerSecond++;
}
#endif
