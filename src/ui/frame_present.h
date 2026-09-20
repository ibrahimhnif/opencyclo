#pragma once

// The one place a composed frame reaches the panel. Every renderer that draws
// into `canvas` ends its frame with presentFrame() instead of calling
// canvas.pushSprite() itself, so screen capture (BLE device command 0x05) sees
// every screen: a pending request is written to the SD card from the composed
// frame first, then a short "saved ..." banner is overlaid for two seconds --
// the banner never ends up in the file. Runs on the UI task, which owns the
// canvas; SD access goes through SdGuard like every other UI-side user.
void presentFrame();
