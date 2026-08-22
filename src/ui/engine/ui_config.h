#ifndef OPENCYCLO_UI_ENGINE_UI_CONFIG_H
#define OPENCYCLO_UI_ENGINE_UI_CONFIG_H

#include "widget_types.h"
#include "template_engine.h"
#include <stddef.h>

#define MAX_PAGES 8
#define MAX_SLOTS_PER_PAGE 8
#define UI_CONFIG_SCHEMA_VERSION 1u

struct PageConfig {
  char title[16];
  LayoutTemplateId template_id;
  uint8_t widget_count;
  WidgetType widgets[MAX_SLOTS_PER_PAGE];
};

struct UiConfig {
  uint32_t schema_version;
  uint8_t active_page_count;
  PageConfig pages[MAX_PAGES];
};

// Pure validation: does this loaded config match the current firmware's
// schema and have sane bounds? No NVS/Arduino dependency — natively testable.
bool isUiConfigValid(const UiConfig& cfg, size_t bytesRead);

#endif // OPENCYCLO_UI_ENGINE_UI_CONFIG_H
