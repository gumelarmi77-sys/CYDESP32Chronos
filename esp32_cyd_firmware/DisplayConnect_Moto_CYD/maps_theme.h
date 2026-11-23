#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "nav_types.h"

enum MapsTheme : uint8_t {
  MAPS_THEME_LIGHT = 0, // Automotive Daylight (Natural Google Maps Light)
  MAPS_THEME_DARK  = 1, // Automotive Night Mode (#12161F Dark Slate + Electric Blue / Neon Orange)
};

void mapsThemeInit();
MapsTheme mapsThemeGet();
bool mapsThemeIsDark();
void mapsThemeSet(MapsTheme theme);
bool mapsThemeToggle();

// LDR Ambient Light Sensor (GPIO 34 on CYD) auto Day/Night with hysteresis
bool mapsThemeAutoGet();
void mapsThemeAutoSet(bool enabled);
bool mapsThemePollLdr(uint32_t nowMs);
uint16_t mapsThemeLastLdrAdc();

DisplayViewMode motoViewModeGet();
void motoViewModeSet(DisplayViewMode mode);
DisplayViewMode motoViewModeToggle();

uint8_t backlightLevelGet();
uint8_t backlightCycleLevel();
uint8_t backlightPwmDuty();
uint8_t backlightCurrentDuty();
uint8_t backlightTickFade(float dt, uint32_t nowMs, bool& outChanged);

// Automotive Cartography & Head-Unit RGB565 Palette
uint16_t mapsColLand();
uint16_t mapsColPark();
uint16_t mapsColGrid();
uint16_t mapsColRoadEdge();
uint16_t mapsColRoadFill();
uint16_t mapsColRouteEdge();
uint16_t mapsColRoute();
uint16_t mapsColRouteDeep();
uint16_t mapsColCard();
uint16_t mapsColCardAlt();
uint16_t mapsColCardBorder();
uint16_t mapsColCardShadow();
uint16_t mapsColText();
uint16_t mapsColMuted();
uint16_t mapsColAccent();
uint16_t mapsColWhite();
uint16_t mapsColSwitchTrack();
uint16_t mapsColSwitchKnob();
uint16_t mapsColDanger();
uint16_t mapsColSuccess();
uint16_t mapsColMonoEye();
