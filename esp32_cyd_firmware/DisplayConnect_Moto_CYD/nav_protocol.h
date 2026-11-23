#pragma once
#include <stddef.h>
#include "nav_types.h"

bool parseNavJson(const char* json, size_t length, NavState& state);
bool parseNotifyJson(const char* json, size_t length, NotifyState& notify);
bool parseMediaJson(const char* json, size_t length, MediaState& media);
bool parseConfigLangJson(const char* json, size_t length, DisplayLang& outLang);
bool isLoadingJson(const char* json, size_t length);
DisplayLang parseLangString(const char* langStr);
