#ifndef OPENCYCLO_STORAGE_SCREENSHOT_H
#define OPENCYCLO_STORAGE_SCREENSHOT_H

#include <stdint.h>
#include <stddef.h>
#include "core/telemetry_state.h"

// Screen capture to /screenshots on the SD card as an uncompressed 24-bit BMP.
// The BLE command handler only raises a request flag; the UI task, which owns
// the composed frame, performs the capture and the SD write (see
// ui/screenshot_hook.h). Everything here is hardware-independent so the BMP
// layout, file naming and flag semantics run under the host test suite.
namespace screenshot {

constexpr int WIDTH = 240;
constexpr int HEIGHT = 320;
constexpr size_t HEADER_BYTES = 54;
constexpr size_t ROW_BYTES = size_t(WIDTH) * 3; // 720: already a multiple of 4, no padding
constexpr size_t FILE_BYTES = HEADER_BYTES + ROW_BYTES * size_t(HEIGHT);

// BITMAPFILEHEADER + BITMAPINFOHEADER for a bottom-up, 24-bit, uncompressed image.
void writeBmpHeader(uint8_t out[HEADER_BYTES], int width, int height);
// One row of R,G,B bytes (as LovyanGFX readRectRGB returns them) to BMP's B,G,R.
void rgbRowToBmp(const uint8_t* rgb, int width, uint8_t* out);

// Rows per SD write: 40 writes per capture instead of one per row.
constexpr int CHUNK_ROWS = 8;

// Fills `rgb` with rows*WIDTH*3 bytes (R,G,B per pixel) for image rows
// y .. y+rows-1, top row first; y = 0 is the top of the screen.
typedef void (*RowReader)(int y, int rows, uint8_t* rgb, void* ctx);

enum Result { SAVED, NO_SD, OPEN_FAILED, WRITE_FAILED };

// Writes /screenshots/<name>.bmp, named by the GPS clock when one is available
// and by uptime seconds otherwise. Never overwrites: an existing name gets the
// same _1, _2 suffix policy as ride files. `path` receives the chosen file path.
Result save(const TelemetryState& state, bool sdReady, RowReader read, void* ctx,
            char* path, size_t pathLen);

} // namespace screenshot

// Cross-task request flag: any task may raise it; the UI task consumes it.
void requestScreenshot();
// True exactly once per raised request, then clears it.
bool takeScreenshotRequest();
// Path of the most recent successful save this boot, "" when none. Offered to
// the app's "download screenshot" (control opcodes 0x16 info / 0x17 open).
const char* lastScreenshotPath();

#endif // OPENCYCLO_STORAGE_SCREENSHOT_H
