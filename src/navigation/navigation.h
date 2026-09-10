#pragma once
#include "core/telemetry_state.h"
#include <NimBLEDevice.h>
void initNavigationService(NimBLEService* service);
void abortRouteTransfer();
void tickRouteTransfer();
// Dedicated map screen, entered from the status bar; horizontal drags pan here.
bool navigationOpen();
void openNavigation();
void navigationGesture(int x0, int y0, int x1, int y1);
void navigationTouch(bool touched, int x, int y);
void cancelNavigationTouch();
void updateNavigation(const TelemetryState& state);
void renderNavigation(const TelemetryState& state);
