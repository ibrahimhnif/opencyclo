#ifndef OPENCYCLO_UI_ENGINE_LAYOUT_MANAGER_H
#define OPENCYCLO_UI_ENGINE_LAYOUT_MANAGER_H

#include "widget_types.h"
#include "template_engine.h"
#include "widget_registry.h"
#include "ui_config.h"
#include "core/telemetry_state.h"

void renderPage(const PageConfig& page, uint8_t pageIdx, uint8_t totalPages, const TelemetryState& state, bool forceFullRedraw);
bool handlePageTouch(const PageConfig& page, int16_t x, int16_t y);

#endif // OPENCYCLO_UI_ENGINE_LAYOUT_MANAGER_H
