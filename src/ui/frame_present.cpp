#include "frame_present.h"
#include "hardware/display.h"
#include "core/telemetry_state.h"
#include "storage/screenshot.h"
#include "storage/sd_access.h"
#include "colors.h"
#include <stdio.h>
#include <string.h>

static char s_notice[48] = {0};
static uint32_t s_noticeUntil = 0;

static void readCanvasRows(int y, int rows, uint8_t* rgb, void*) {
  canvas.readRectRGB(0, y, screenshot::WIDTH, rows, rgb); // R,G,B per pixel, top row first
}

static void captureIfRequested() {
  if (!takeScreenshotRequest()) return;
  char path[64] = {0};
  {
    SdGuard sd;
    if (!sd.locked) {
      snprintf(s_notice, sizeof(s_notice), "screenshot: SD busy");
    } else {
      const TelemetryState state = getTelemetrySnapshot();
      switch (screenshot::save(state, g_sd_ready, readCanvasRows, nullptr, path, sizeof(path))) {
        case screenshot::SAVED: {
          const char* slash = strrchr(path, '/');
          snprintf(s_notice, sizeof(s_notice), "saved %s", slash ? slash + 1 : path);
          break;
        }
        case screenshot::NO_SD:       snprintf(s_notice, sizeof(s_notice), "screenshot: no SD card"); break;
        case screenshot::OPEN_FAILED: snprintf(s_notice, sizeof(s_notice), "screenshot: SD open failed"); break;
        default:                      snprintf(s_notice, sizeof(s_notice), "screenshot: SD write failed"); break;
      }
    }
  }
  s_noticeUntil = millis() + 2000;
  Serial.printf("[SCREENSHOT] %s\n", s_notice);
}

static void drawNotice() {
  if (!s_notice[0] || (int32_t)(millis() - s_noticeUntil) >= 0) return;
  const int x = 8, y = 276, w = 224, h = 26;
  canvas.fillRoundRect(x, y, w, h, 6, ui::panel);
  canvas.drawRoundRect(x, y, w, h, 6, ui::accent);
  canvas.setFont(&fonts::Font0);
  canvas.setTextColor(TFT_WHITE, ui::panel);
  canvas.drawString(s_notice, x + 10, y + 9);
}

void presentFrame() {
  captureIfRequested();
  drawNotice();
  canvas.pushSprite(0, 0);
}
