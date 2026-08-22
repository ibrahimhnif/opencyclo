#ifndef OPENCYCLO_UI_ENGINE_WIDGET_REGISTRY_H
#define OPENCYCLO_UI_ENGINE_WIDGET_REGISTRY_H

#include "widget_types.h"
#include "template_engine.h"
#include "core/telemetry_state.h"
#include "hardware/display.h"

typedef void (*WidgetRenderFn)(const Rect& bounds, const TelemetryState& state, bool forceFullRedraw);
typedef bool (*WidgetTouchFn)(const Rect& bounds, int16_t x, int16_t y);

struct WidgetDescriptor {
  WidgetType type;
  WidgetRenderFn render_fn;
  WidgetTouchFn touch_fn;
};

void initWidgetRegistry();
const WidgetDescriptor* getWidgetDescriptor(WidgetType type);

// Renders `type` into `slot` only if the widget supports slot.size_class;
// otherwise draws a blank placeholder tile (background fill, no text) so an
// invalid config can never produce overlapping/garbled text again.
void renderWidget(WidgetType type, const TemplateSlot& slot, const TelemetryState& state, bool forceFullRedraw);
bool handleWidgetTouch(WidgetType type, const Rect& bounds, int16_t x, int16_t y);

#endif // OPENCYCLO_UI_ENGINE_WIDGET_REGISTRY_H
