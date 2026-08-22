#ifndef OPENCYCLO_UI_ENGINE_WIDGET_TYPES_H
#define OPENCYCLO_UI_ENGINE_WIDGET_TYPES_H

#include <stdint.h>

// A slot's SizeClass is the MINIMUM size a widget must support to be placed
// there. A widget with more room than it needs is fine; less is not.
enum SizeClass : uint8_t {
  SIZE_SMALL = 0,  // 74x54   - bottom-row tiles
  SIZE_MEDIUM,     // 114x60  - mid-row tiles
  SIZE_LARGE,      // 232x192 - the chart slot in TEMPLATE_2_GRID_CHART
  SIZE_HERO,       // 232x94  - the single headline tile (Speed)
  SIZE_FULL,       // 232x274 - whole-page management screens
  SIZE_CLASS_COUNT
};

enum WidgetType : uint8_t {
  WIDGET_NONE = 0,
  WIDGET_SPEED,
  WIDGET_AVG_SPEED,
  WIDGET_MAX_SPEED,
  WIDGET_DISTANCE,
  WIDGET_RIDE_TIME,
  WIDGET_CADENCE,
  WIDGET_HEART_RATE,
  WIDGET_POWER,
  WIDGET_ALTITUDE,
  WIDGET_GRADE,
  WIDGET_TOTAL_ASCENT,
  WIDGET_ELEVATION_CHART,
  WIDGET_BATTERY,
  WIDGET_BLE_MANAGER,
  WIDGET_SETTINGS_LIST,
  WIDGET_TYPE_COUNT
};

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

#endif // OPENCYCLO_UI_ENGINE_WIDGET_TYPES_H
