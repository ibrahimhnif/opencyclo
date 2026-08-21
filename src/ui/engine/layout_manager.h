#ifndef OPENCYCLO_UI_ENGINE_LAYOUT_MANAGER_H
#define OPENCYCLO_UI_ENGINE_LAYOUT_MANAGER_H

#include "widget_types.h"
#include "template_engine.h"
#include "widget_registry.h"
#include "core/telemetry_state.h"

#define MAX_PAGES 8
#define MAX_SLOTS_PER_PAGE 8

struct PageConfig {
  char title[16];
  LayoutTemplateId template_id;
  uint8_t widget_count;
  WidgetType widgets[MAX_SLOTS_PER_PAGE];
};

struct UiConfig {
  uint8_t active_page_count;
  PageConfig pages[MAX_PAGES];
};

void renderPage(const PageConfig& page, uint8_t pageIdx, uint8_t totalPages, const TelemetryState& state, bool forceFullRedraw);
bool handlePageTouch(const PageConfig& page, int16_t x, int16_t y);

#endif // OPENCYCLO_UI_ENGINE_LAYOUT_MANAGER_H
