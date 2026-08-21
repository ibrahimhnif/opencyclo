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

    // Render active dynamic page from UiConfig
    if (g_ui_config.active_page_count > 0) {
      renderPage(g_ui_config.pages[currentPageIdx], currentPageIdx, g_ui_config.active_page_count, state, forceRedraw);
    }

    if (forceRedraw) {
      forceRedraw = false;
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // ~20 FPS rendering loop
  }
}
