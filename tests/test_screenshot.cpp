#include "gpx_fakes.h"
#include "storage/screenshot.h"
#include <assert.h>
#include <string>

GpxSerial Serial;
FakeCard SD_MMC;
size_t writeLimit=SIZE_MAX;

static uint32_t le32(const std::string& s, size_t at) {
  return uint32_t(uint8_t(s[at])) | uint32_t(uint8_t(s[at+1]))<<8 |
         uint32_t(uint8_t(s[at+2]))<<16 | uint32_t(uint8_t(s[at+3]))<<24;
}
static uint16_t le16(const std::string& s, size_t at) {
  return uint16_t(uint8_t(s[at])) | uint16_t(uint8_t(s[at+1]))<<8;
}

// Row y is filled with R=y&0xFF, G=7, B=9 so the file's row order is checkable.
static void patternRow(int y, int rows, uint8_t* rgb, void*) {
  for (int r = 0; r < rows; ++r)
    for (int x = 0; x < screenshot::WIDTH; ++x) {
      uint8_t* p = rgb + size_t(r) * screenshot::ROW_BYTES + size_t(x) * 3;
      p[0] = uint8_t(y + r); p[1] = 7; p[2] = 9;
    }
}

int main() {
  // 54-byte BITMAPFILEHEADER + BITMAPINFOHEADER, bottom-up 24-bit, no compression.
  uint8_t h[screenshot::HEADER_BYTES];
  screenshot::writeBmpHeader(h, 240, 320);
  const std::string hs(reinterpret_cast<const char*>(h), sizeof(h));
  assert(hs[0]=='B' && hs[1]=='M');
  assert(le32(hs, 2) == screenshot::FILE_BYTES);
  assert(le32(hs, 10) == 54);
  assert(le32(hs, 14) == 40);
  assert(le32(hs, 18) == 240);
  assert(le32(hs, 22) == 320);
  assert(le16(hs, 26) == 1);
  assert(le16(hs, 28) == 24);
  assert(le32(hs, 30) == 0);
  assert(le32(hs, 34) == 240u*3u*320u);
  assert(screenshot::ROW_BYTES % 4 == 0); // 720: no BMP row padding needed

  // R,G,B in (as LovyanGFX readRectRGB returns) -> B,G,R out.
  const uint8_t rgb[6] = {0xFF, 0x00, 0x00, 1, 2, 3};
  uint8_t bgr[6] = {0};
  screenshot::rgbRowToBmp(rgb, 2, bgr);
  assert(bgr[0]==0x00 && bgr[1]==0x00 && bgr[2]==0xFF);
  assert(bgr[3]==3 && bgr[4]==2 && bgr[5]==1);

  // Request flag: one take per request, idempotent double request.
  assert(!takeScreenshotRequest());
  requestScreenshot(); requestScreenshot();
  assert(takeScreenshotRequest());
  assert(!takeScreenshotRequest());

  // Nothing saved yet: no "last screenshot" to offer for download.
  assert(lastScreenshotPath()[0] == 0);

  // Saved under the GPS clock name; rows are written bottom-up.
  TelemetryState state;
  state.gps_year=2026; state.gps_month=9; state.gps_day=20;
  state.gps_hour=8; state.gps_minute=30; state.gps_second=12;
  char path[64]={0};
  assert(screenshot::save(state, true, patternRow, nullptr, path, sizeof(path)) == screenshot::SAVED);
  assert(std::string(path) == "/screenshots/20260920_083012.bmp");
  assert(std::string(lastScreenshotPath()) == path);
  const std::string f = SD_MMC.files[path];
  assert(f.size() == screenshot::FILE_BYTES);
  assert(f.substr(0, 54) == hs);
  // First stored row is the bottom image row (y=319): B=9, G=7, R=319&0xFF=63.
  assert(uint8_t(f[54])==9 && uint8_t(f[55])==7 && uint8_t(f[56])==63);
  // Last stored row is the top image row (y=0).
  const size_t lastRow = 54 + screenshot::ROW_BYTES*319;
  assert(uint8_t(f[lastRow])==9 && uint8_t(f[lastRow+1])==7 && uint8_t(f[lastRow+2])==0);

  // Same clock never overwrites: suffix like ride files.
  assert(screenshot::save(state, true, patternRow, nullptr, path, sizeof(path)) == screenshot::SAVED);
  assert(std::string(path) == "/screenshots/20260920_083012_1.bmp");

  // No GPS clock: uptime seconds (fake millis() is 42000).
  TelemetryState noClock;
  assert(screenshot::save(noClock, true, patternRow, nullptr, path, sizeof(path)) == screenshot::SAVED);
  assert(std::string(path) == "/screenshots/shot_42.bmp");
  assert(screenshot::save(noClock, true, patternRow, nullptr, path, sizeof(path)) == screenshot::SAVED);
  assert(std::string(path) == "/screenshots/shot_42_1.bmp");

  // Failure paths leave no partial claims of success.
  const size_t before = SD_MMC.files.size();
  assert(screenshot::save(state, false, patternRow, nullptr, path, sizeof(path)) == screenshot::NO_SD);
  assert(SD_MMC.files.size() == before);
  // A failed save keeps offering the last file that really exists.
  assert(std::string(lastScreenshotPath()) == "/screenshots/shot_42_1.bmp");
  SD_MMC.failOpen = true;
  assert(screenshot::save(state, true, patternRow, nullptr, path, sizeof(path)) == screenshot::OPEN_FAILED);
  SD_MMC.failOpen = false;
  writeLimit = 100;
  assert(screenshot::save(state, true, patternRow, nullptr, path, sizeof(path)) == screenshot::WRITE_FAILED);
  writeLimit = SIZE_MAX;

  puts("Screenshot BMP header, row order, naming and request flag tests passed");
}
