#include "ui_task.h"
#include "hardware/display.h"
#include "core/telemetry_state.h"
#include "pages/ride_page.h"
#include "pages/climb_page.h"
#include "pages/sensors_page.h"
#include "pages/debug_page.h"
#include "pages/settings_page.h"
#include "storage/settings.h"
#include <stdlib.h>

static UiPage currentUiPage = PAGE_RIDE;
static UiPage activePageDrawn = PAGE_RIDE;
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

        Serial.printf("[GESTURE] Touch release. Start:(%d,%d), End:(%d,%d), DeltaX:%d, DeltaY:%d\n",
                      touchStartX, touchStartY, lastTouchX, lastTouchY, deltaX, deltaY);

        // Gesture 1: SWIPE LEFT (Next Page)
        if (deltaX < -35 && abs(deltaY) < 70) {
          currentUiPage = (UiPage)((currentUiPage + 1) % PAGE_COUNT);
          Serial.printf("[UI GESTURE] Swiped Left -> New Page: %d\n", currentUiPage);
          forceRedraw = true;
        }
        // Gesture 2: SWIPE RIGHT (Previous Page)
        else if (deltaX > 35 && abs(deltaY) < 70) {
          currentUiPage = (UiPage)((currentUiPage + PAGE_COUNT - 1) % PAGE_COUNT);
          Serial.printf("[UI GESTURE] Swiped Right -> New Page: %d\n", currentUiPage);
          forceRedraw = true;
        }
        // Gesture 3: SINGLE TAP / PRESS (Page Action Controls)
        else if (abs(deltaX) < 20 && abs(deltaY) < 20) {
          int16_t tapX = lastTouchX;
          int16_t tapY = lastTouchY;

          if (currentUiPage == PAGE_RIDE) {
            if (handleRidePageTouch(tapX, tapY)) {
              Serial.println("[UI] Ride button tapped");
              forceRedraw = true;
            }
          } else if (currentUiPage == PAGE_GPS_INFO) {
            if (handleSensorsPageTouch(tapX, tapY)) {
              Serial.println("[UI] Sensor button tapped");
              forceRedraw = true;
            }
          } else if (currentUiPage == PAGE_SETTINGS) {
            if (handleSettingsPageTouch(tapX, tapY)) {
              Serial.println("[UI] Setting button tapped");
              forceRedraw = true;
            }
          }
        }
      }

      touchStartX = -1;
      touchStartY = -1;
    }

    // Check page switch
    if (activePageDrawn != currentUiPage) {
      forceRedraw = true;
      activePageDrawn = currentUiPage;
    }

    // Fetch snapshot of telemetry state
    TelemetryState state = getTelemetrySnapshot();

    // Render active UI page
    if (currentUiPage == PAGE_RIDE) {
      renderRidePage(state, forceRedraw);
    } else if (currentUiPage == PAGE_CLIMB) {
      renderClimbPage(state, forceRedraw);
    } else if (currentUiPage == PAGE_GPS_INFO) {
      renderSensorsPage(state, forceRedraw);
    } else if (currentUiPage == PAGE_DEBUG) {
      renderDebugPage(state, forceRedraw);
    } else if (currentUiPage == PAGE_SETTINGS) {
      renderSettingsPage(state, forceRedraw);
    }

    if (forceRedraw) {
      forceRedraw = false;
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // ~20 FPS rendering loop
  }
}
