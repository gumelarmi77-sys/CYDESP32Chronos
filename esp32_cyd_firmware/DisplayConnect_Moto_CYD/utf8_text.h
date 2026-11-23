#pragma once
#include <TFT_eSPI.h>
#include "nav_types.h"

int utf8TextWidth(const char* text);
void drawUtf8Text(TFT_eSPI& tft, const char* text, int x, int y, int maxWidth, uint16_t color, uint16_t background);

DisplayLang displayLangGet();
void setDisplayLang(DisplayLang lang);
void initDisplayLanguage();
