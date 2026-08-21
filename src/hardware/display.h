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

void initDisplay();
void setDisplayBrightness(uint8_t duty);

#endif // OPENCYCLO_HARDWARE_DISPLAY_H
