#pragma once
#include <stdint.h>
#include "config.h"
#include "nav_types.h"

#if ENABLE_CHRONOS_APP

struct ChronosBridgeOutput {
  volatile bool* clientConnected;
  volatile bool* awaitingFirstNav;
  volatile bool* uiShowLoading;
  volatile bool* uiShowWaiting;
  bool* hasNav;
  bool* showingWaiting;
  NavState* lastNav;
  NotifyState* activeNotify;
  MediaState* lastMedia;
  const HwGpsData* hwGps;
  void (*onNavUpdated)(bool firstPacket);
  void (*onNotifyUpdated)();
  void (*onNavStopped)();
};

bool chronosBridgeInit(const ChronosBridgeOutput& out);
void chronosBridgeLoop(uint32_t nowMs);
void chronosBridgeSendMediaCmd(MediaCommand cmd);
void chronosBridgeDisconnect();

#endif
