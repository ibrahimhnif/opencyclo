#pragma once
#include "ui/engine/widget_types.h"
#include <stdint.h>

// The Insta360 camera widget (WIDGET_CAMERA_REMOTE): Scan -> Modes -> Capture
// (+ More). Page logic and geometry live in camera_view.h; camera I/O in
// hardware/camera_control.h. Registered in widget_registry.cpp.
void renderCameraWidget(const Rect& b);
bool touchCameraWidget(const Rect& b, int16_t x, int16_t y);
