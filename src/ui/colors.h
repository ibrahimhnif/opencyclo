#pragma once
#include <cstdint>
namespace ui {
// RGB565 quantizations of the approved design-system sRGB tokens.
constexpr uint16_t rgb565(uint8_t r,uint8_t g,uint8_t b){return uint16_t((r>>3)<<11|(g>>2)<<5|(b>>3));}
constexpr uint16_t accent=rgb565(0,210,255),panel=rgb565(22,26,32);
constexpr uint16_t muted=rgb565(168,177,189),success=rgb565(46,213,115);
constexpr uint16_t warning=rgb565(255,171,0),danger=rgb565(255,105,116);
}
