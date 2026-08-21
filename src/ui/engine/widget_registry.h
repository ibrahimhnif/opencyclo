#ifndef OPENCYCLO_UI_ENGINE_WIDGET_REGISTRY_H
#define OPENCYCLO_UI_ENGINE_WIDGET_REGISTRY_H

#include "widget_types.h"
#include "core/telemetry_state.h"
#include "hardware/display.h"

typedef void (*WidgetRenderFn)(const Rect& bounds, const TelemetryState& state, bool forceFullRedraw);
typedef bool (*WidgetTouchFn)(const Rect& bounds, int16_t x, int16_t y);

struct WidgetDescriptor {
  WidgetType type;
  const char* name;
  const char* label;
  const char* unit_metric;
  const char* unit_imperial;
  WidgetRenderFn render_fn;
  WidgetTouchFn touch_fn;
};

void initWidgetRegistry();
const WidgetDescriptor* getWidgetDescriptor(WidgetType type);
void renderWidget(WidgetType type, const Rect& bounds, const TelemetryState& state, bool forceFullRedraw);
bool handleWidgetTouch(WidgetType type, const Rect& bounds, int16_t x, int16_t y);

#endif // OPENCYCLO_UI_ENGINE_WIDGET_REGISTRY_H
