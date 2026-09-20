#include "screenshot.h"
#include "unique_path.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <stdio.h>
#include <string.h>

static volatile bool s_requested = false;
static char s_lastPath[64] = {0};

const char* lastScreenshotPath() { return s_lastPath; }

void requestScreenshot() { s_requested = true; }

bool takeScreenshotRequest() {
  if (!s_requested) return false;
  s_requested = false;
  return true;
}

namespace screenshot {

static void put32(uint8_t* p, uint32_t v) {
  p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); p[2] = uint8_t(v >> 16); p[3] = uint8_t(v >> 24);
}
static void put16(uint8_t* p, uint16_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); }

void writeBmpHeader(uint8_t out[HEADER_BYTES], int width, int height) {
  const uint32_t rowBytes = ((uint32_t(width) * 3 + 3) / 4) * 4;
  const uint32_t imageBytes = rowBytes * uint32_t(height);
  memset(out, 0, HEADER_BYTES);
  out[0] = 'B'; out[1] = 'M';
  put32(out + 2, uint32_t(HEADER_BYTES) + imageBytes); // file size
  put32(out + 10, uint32_t(HEADER_BYTES));             // pixel data offset
  put32(out + 14, 40);                                 // BITMAPINFOHEADER size
  put32(out + 18, uint32_t(width));
  put32(out + 22, uint32_t(height));                   // positive: bottom-up rows
  put16(out + 26, 1);                                  // planes
  put16(out + 28, 24);                                 // bits per pixel
  put32(out + 30, 0);                                  // BI_RGB, no compression
  put32(out + 34, imageBytes);
}

void rgbRowToBmp(const uint8_t* rgb, int width, uint8_t* out) {
  for (int x = 0; x < width; ++x) {
    out[x * 3]     = rgb[x * 3 + 2];
    out[x * 3 + 1] = rgb[x * 3 + 1];
    out[x * 3 + 2] = rgb[x * 3];
  }
}

Result save(const TelemetryState& state, bool sdReady, RowReader read, void* ctx,
            char* path, size_t pathLen) {
  if (!sdReady) return NO_SD;
  storage::ensureDir("/screenshots");
  char base[48];
  storage::timestampedBase(base, sizeof(base), "/screenshots", "shot",
                           state.gps_year, state.gps_month, state.gps_day,
                           state.gps_hour, state.gps_minute, state.gps_second, millis() / 1000);
  storage::nextFreePath(path, pathLen, base, ".bmp");

  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) return OPEN_FAILED;

  uint8_t header[HEADER_BYTES];
  writeBmpHeader(header, WIDTH, HEIGHT);
  bool ok = f.write(header, HEADER_BYTES) == HEADER_BYTES;

  // Static: the UI task's stack only carries the path while ~11 KB of rows
  // stream through here. BMP stores the bottom row first, so walk upward.
  static uint8_t rgb[CHUNK_ROWS * ROW_BYTES], bgr[CHUNK_ROWS * ROW_BYTES];
  for (int bottom = HEIGHT; ok && bottom > 0;) {
    const int rows = bottom < CHUNK_ROWS ? bottom : CHUNK_ROWS;
    const int top = bottom - rows;
    read(top, rows, rgb, ctx); // canvas order: top row first
    for (int k = 0; k < rows; ++k)
      rgbRowToBmp(rgb + size_t(rows - 1 - k) * ROW_BYTES, WIDTH, bgr + size_t(k) * ROW_BYTES);
    const size_t bytes = size_t(rows) * ROW_BYTES;
    ok = f.write(bgr, bytes) == bytes;
    bottom = top;
  }
  f.close();
  if (!ok) return WRITE_FAILED;
  snprintf(s_lastPath, sizeof(s_lastPath), "%s", path);
  return SAVED;
}

} // namespace screenshot
