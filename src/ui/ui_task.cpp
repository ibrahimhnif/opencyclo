#include "ui_task.h"
#include "hardware/display.h"
#include "core/telemetry_state.h"
#include "engine/layout_manager.h"
#include "storage/layout_config.h"
#include "storage/settings.h"
#include <stdlib.h>

static uint8_t currentPageIdx = 0;
static uint8_t activePageDrawn = 255;
static bool forceRedraw = true;

void startUiTask() {
  xTaskCreatePinnedToCore(
    uiTaskLoop,
    "UiTask",
    8192,
    NULL,
    1, // Priority 1
    NULL,
    1  // Core 1
  );
}

void uiTaskLoop(void* pvParameters) {
  initDisplay();
  setDisplayBrightness(g_settings.brightness);
  forceRedraw = true;

  int16_t touchStartX = -1, touchStartY = -1;
  int16_t lastTouchX = -1, lastTouchY = -1;
  bool wasTouched = false;
  uint32_t lastTouchMs = 0;

  for (;;) {
    uint32_t now = millis();

    // Read touch input from FT6336G
    int16_t x = 0, y = 0;
    bool isTouched = false;

    if (g_i2c_mutex != NULL && xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      isTouched = tft.getTouch(&x, &y);
      xSemaphoreGive(g_i2c_mutex);
    }

    if (isTouched) {
      if (!wasTouched) {
        touchStartX = x;
        touchStartY = y;
        wasTouched = true;
      }
      lastTouchX = x;
      lastTouchY = y;
    } else if (wasTouched) {
      // Touch released! Evaluate gesture (Swipe vs Single Tap)
      wasTouched = false;

      if (now - lastTouchMs > 150 && touchStartX >= 0 && lastTouchX >= 0) {
        lastTouchMs = now;
        int16_t deltaX = lastTouchX - touchStartX;
        int16_t deltaY = lastTouchY - touchStartY;

        uint8_t totalPages = (g_ui_config.active_page_count > 0) ? g_ui_config.active_page_count : 1;

        // Gesture 1: SWIPE LEFT (Next Page)
        if (deltaX < -35 && abs(deltaY) < 70) {
          currentPageIdx = (currentPageIdx + 1) % totalPages;
          Serial.printf("[UI GESTURE] Swiped Left -> Page %u/%u\n", currentPageIdx + 1, totalPages);
          forceRedraw = true;
        }
        // Gesture 2: SWIPE RIGHT (Previous Page)
        else if (deltaX > 35 && abs(deltaY) < 70) {
          currentPageIdx = (currentPageIdx + totalPages - 1) % totalPages;
          Serial.printf("[UI GESTURE] Swiped Right -> Page %u/%u\n", currentPageIdx + 1, totalPages);
          forceRedraw = true;
        }
        // Gesture 3: SINGLE TAP / PRESS (Delegate to current page layout & widgets)
        else if (abs(deltaX) < 20 && abs(deltaY) < 20) {
          if (handlePageTouch(g_ui_config.pages[currentPageIdx], lastTouchX, lastTouchY)) {
            forceRedraw = true;
          }
        }
      }

      touchStartX = -1;
      touchStartY = -1;
    }

    // Check page switch
    if (activePageDrawn != currentPageIdx) {
      forceRedraw = true;
      activePageDrawn = currentPageIdx;
    }

    // Fetch snapshot of telemetry state
    TelemetryState state = getTelemetrySnapshot();

    // Render active dynamic page from UiConfig -- every widget/layout draw
    // call targets the off-screen canvas sprite, not the panel directly.
    if (g_ui_config.active_page_count > 0) {
      renderPage(g_ui_config.pages[currentPageIdx], currentPageIdx, g_ui_config.active_page_count, state, forceRedraw);
    }

    // Blit the fully-composed frame to the panel in one SPI transfer. Doing
    // this every iteration (not just on forceRedraw) is what makes double
    // buffering actually work: the dynamic per-frame text updates inside
    // renderPage() above still only touch canvas, so without this the panel
    // would never see them. Measured at ~19-20ms on this hardware (240x320
    // RGB565, ~153KB) at the 80MHz SPI clock set in display.cpp.
    canvas.pushSprite(0, 0);

    if (forceRedraw) {
      forceRedraw = false;
    }

    // Was a flat 50ms (~20Hz) added on top of the loop body's own cost, back
    // when that cost was negligible (direct-to-panel drawing, no full-frame
    // push). Double buffering's canvas.pushSprite() alone now costs ~19-20ms
    // (measured, after also bumping the SPI clock 40->80MHz -- see
    // display.cpp), so 50ms of *additional* delay was stacking on top of an
    // already-~35ms loop body, roughly halving the real update rate.
    // Reduced to 15ms so total period lands back near the original ~50ms/
    // ~20Hz target instead of drifting to ~85ms/~12Hz.
    vTaskDelay(pdMS_TO_TICKS(15));
  }
}
