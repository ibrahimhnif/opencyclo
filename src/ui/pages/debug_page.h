#ifndef OPENCYCLO_UI_PAGES_DEBUG_PAGE_H
#define OPENCYCLO_UI_PAGES_DEBUG_PAGE_H

#include "hardware/display.h"
#include "core/telemetry_state.h"

void renderDebugPage(const TelemetryState& state, bool forceFullRedraw);

#endif // OPENCYCLO_UI_PAGES_DEBUG_PAGE_H
