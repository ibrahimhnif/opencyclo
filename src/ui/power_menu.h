#pragma once
#include <stdint.h>

void openPowerMenu();
// True means this frame belongs to the power UI, not the ride page.
bool updatePowerUi(bool touched, int16_t x, int16_t y, uint32_t now);
