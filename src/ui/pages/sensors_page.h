#ifndef OPENCYCLO_UI_PAGES_SENSORS_PAGE_H
#define OPENCYCLO_UI_PAGES_SENSORS_PAGE_H

#include "hardware/display.h"
#include "core/telemetry_state.h"

void renderSensorsPage(const TelemetryState& state, bool forceFullRedraw);
bool handleSensorsPageTouch(int16_t x, int16_t y);

#endif // OPENCYCLO_UI_PAGES_SENSORS_PAGE_H
