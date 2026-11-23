#pragma once
#include <stdint.h>
#include <stdbool.h>

bool touchInit();
bool touchRead(uint16_t& x, uint16_t& y);
