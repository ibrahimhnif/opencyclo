#ifndef OPENCYCLO_UI_ENGINE_WIDGET_CATALOG_H
#define OPENCYCLO_UI_ENGINE_WIDGET_CATALOG_H

#include "widget_types.h"

struct WidgetMeta {
  WidgetType type;
  const char* name;
  const char* label;
  const char* unit_metric;
  const char* unit_imperial;
  uint8_t supported_sizes_mask; // bitmask of (1 << SizeClass)
};

const WidgetMeta& getWidgetMeta(WidgetType type);
bool widgetSupportsSize(WidgetType type, SizeClass size);

#endif // OPENCYCLO_UI_ENGINE_WIDGET_CATALOG_H
