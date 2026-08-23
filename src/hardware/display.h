#ifndef OPENCYCLO_HARDWARE_DISPLAY_H
#define OPENCYCLO_HARDWARE_DISPLAY_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "config/pins.h"

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI       _bus_instance;
  lgfx::Light_PWM     _light_instance;
  lgfx::Touch_FT5x06  _touch_instance;

public:
  LGFX();
};

extern LGFX tft;

// Full-screen off-screen framebuffer (240x320, RGB565, allocated in PSRAM --
// this board's ESP32-S3 N16R8 has 8MB, so the ~150KB buffer is trivial).
// Every widget/layout render function draws to this sprite instead of
// directly to tft; ui_task.cpp pushes the whole composed frame to the panel
// in one SPI transfer via canvas.pushSprite() each loop iteration. Without
// this, each draw call wrote straight to the panel over SPI, so a photo or
// video could occasionally catch the screen mid-write (a partially-drawn
// glyph, old and new text visible in the same frame) -- normal for a
// single-buffered SPI TFT, but avoidable with double buffering.
// Touch reads always go through the real tft object, never canvas -- a
// sprite has no touch controller.
extern LGFX_Sprite canvas;

void initDisplay();
void setDisplayBrightness(uint8_t duty);

#endif // OPENCYCLO_HARDWARE_DISPLAY_H
