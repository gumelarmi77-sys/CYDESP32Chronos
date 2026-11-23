#pragma once
#include <TFT_eSPI.h>
#include <stdint.h>

void playBootSplashSweep(TFT_eSPI& tft);
void showMapLoadingScreen(TFT_eSPI& tft);
void updateMapLoadingAnimation(TFT_eSPI& tft, uint32_t nowMs);
void freeMapLoadingSprite();
