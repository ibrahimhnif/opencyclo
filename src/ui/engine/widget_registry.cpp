#include "widget_registry.h"
#include "widget_catalog.h"

static uint16_t COLOR_BG = TFT_BLACK;

// Stub renderer used for every widget until Tasks 6-10 replace it with the
// real Minimal-style implementation. Draws only the background fill so the
// dispatch/validation plumbing in this task is independently verifiable.
static void renderStub(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
  }
}

static const WidgetDescriptor s_descriptors[WIDGET_TYPE_COUNT] = {
  {WIDGET_NONE,            nullptr,    nullptr},
  {WIDGET_SPEED,           renderStub, nullptr},
  {WIDGET_AVG_SPEED,       renderStub, nullptr},
  {WIDGET_MAX_SPEED,       renderStub, nullptr},
  {WIDGET_DISTANCE,        renderStub, nullptr},
  {WIDGET_RIDE_TIME,       renderStub, nullptr},
  {WIDGET_CADENCE,         renderStub, nullptr},
  {WIDGET_HEART_RATE,      renderStub, nullptr},
  {WIDGET_POWER,           renderStub, nullptr},
  {WIDGET_ALTITUDE,        renderStub, nullptr},
  {WIDGET_GRADE,           renderStub, nullptr},
  {WIDGET_TOTAL_ASCENT,    renderStub, nullptr},
  {WIDGET_ELEVATION_CHART, renderStub, nullptr},
  {WIDGET_BATTERY,         renderStub, nullptr},
  {WIDGET_BLE_MANAGER,     renderStub, nullptr},
  {WIDGET_SETTINGS_LIST,   renderStub, nullptr},
};

void initWidgetRegistry() {
  Serial.println("[WIDGET REGISTRY] Initialized 15 modular widgets (Minimal style rebuild in progress).");
}

const WidgetDescriptor* getWidgetDescriptor(WidgetType type) {
  if (type < WIDGET_TYPE_COUNT) return &s_descriptors[type];
  return &s_descriptors[WIDGET_NONE];
}

void renderWidget(WidgetType type, const TemplateSlot& slot, const TelemetryState& state, bool forceFullRedraw) {
  if (type >= WIDGET_TYPE_COUNT) return;
  if (!widgetSupportsSize(type, slot.size_class)) {
    // Invalid widget/slot pairing (e.g. stale config): fail safe, don't draw garbage.
    if (forceFullRedraw) {
      tft.fillRect(slot.rect.x, slot.rect.y, slot.rect.w, slot.rect.h, COLOR_BG);
    }
    return;
  }
  const WidgetRenderFn fn = s_descriptors[type].render_fn;
  if (fn != nullptr) {
    fn(slot.rect, state, forceFullRedraw);
  }
}

bool handleWidgetTouch(WidgetType type, const Rect& bounds, int16_t x, int16_t y) {
  if (type < WIDGET_TYPE_COUNT && s_descriptors[type].touch_fn != nullptr) {
    return s_descriptors[type].touch_fn(bounds, x, y);
  }
  return false;
}
