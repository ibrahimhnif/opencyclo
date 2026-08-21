#include "ui_task.h"
#include "hardware/display.h"
#include "core/telemetry_state.h"
#include "pages/ride_page.h"
#include "pages/climb_page.h"
#include "pages/sensors_page.h"
#include "pages/debug_page.h"
#include "pages/settings_page.h"
#include "storage/settings.h"

static UiPage currentUiPage = PAGE_RIDE;
static UiPage activePageDrawn = PAGE_RIDE;
static bool forceRedraw = true;

static uint16_t COLOR_BG       = tft.color565(12, 16, 26);
static uint16_t COLOR_CARD     = tft.color565(26, 34, 52);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);
static uint16_t COLOR_TEXT_MUT = tft.color565(140, 155, 180);

static void renderNavBar() {
  tft.fillRect(0, 280, 240, 40, COLOR_BG);
  tft.drawFastHLine(0, 280, 240, COLOR_CARD);

  // Tab 0: RIDE (2..46)
  uint16_t tab0Bg = (currentUiPage == PAGE_RIDE) ? COLOR_CYAN : COLOR_CARD;
  uint16_t tab0Fg = (currentUiPage == PAGE_RIDE) ? TFT_BLACK : COLOR_TEXT_MUT;
  tft.fillRoundRect(2, 284, 44, 32, 4, tab0Bg);
  tft.setTextColor(tab0Fg, tab0Bg);
  tft.setTextSize(1);
  tft.setCursor(12, 296);
  tft.print("RIDE");

  // Tab 1: CLIMB (48..92)
  uint16_t tab1Bg = (currentUiPage == PAGE_CLIMB) ? COLOR_CYAN : COLOR_CARD;
  uint16_t tab1Fg = (currentUiPage == PAGE_CLIMB) ? TFT_BLACK : COLOR_TEXT_MUT;
  tft.fillRoundRect(48, 284, 44, 32, 4, tab1Bg);
  tft.setTextColor(tab1Fg, tab1Bg);
  tft.setTextSize(1);
  tft.setCursor(55, 296);
  tft.print("CLIMB");

  // Tab 2: SENSORS (94..138)
  uint16_t tab2Bg = (currentUiPage == PAGE_GPS_INFO) ? COLOR_CYAN : COLOR_CARD;
  uint16_t tab2Fg = (currentUiPage == PAGE_GPS_INFO) ? TFT_BLACK : COLOR_TEXT_MUT;
  tft.fillRoundRect(94, 284, 44, 32, 4, tab2Bg);
  tft.setTextColor(tab2Fg, tab2Bg);
  tft.setTextSize(1);
  tft.setCursor(100, 296);
  tft.print("SENSR");

  // Tab 3: NMEA DEBUG (140..184)
  uint16_t tab3Bg = (currentUiPage == PAGE_DEBUG) ? COLOR_CYAN : COLOR_CARD;
  uint16_t tab3Fg = (currentUiPage == PAGE_DEBUG) ? TFT_BLACK : COLOR_TEXT_MUT;
  tft.fillRoundRect(140, 284, 44, 32, 4, tab3Bg);
  tft.setTextColor(tab3Fg, tab3Bg);
  tft.setTextSize(1);
  tft.setCursor(148, 296);
  tft.print("NMEA");

  // Tab 4: SETTING (186..238)
  uint16_t tab4Bg = (currentUiPage == PAGE_SETTINGS) ? COLOR_CYAN : COLOR_CARD;
  uint16_t tab4Fg = (currentUiPage == PAGE_SETTINGS) ? TFT_BLACK : COLOR_TEXT_MUT;
  tft.fillRoundRect(186, 284, 52, 32, 4, tab4Bg);
  tft.setTextColor(tab4Fg, tab4Bg);
  tft.setTextSize(1);
  tft.setCursor(192, 296);
  tft.print("SETTNG");
}

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

  int16_t touchX = 0, touchY = 0;
  bool wasTouched = false;
  uint32_t lastTouchMs = 0;

  for (;;) {
    uint32_t now = millis();

    // Read touch input
    int16_t x = 0, y = 0;
    bool isTouched = false;

    if (g_i2c_mutex != NULL && xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      isTouched = tft.getTouch(&x, &y);
      xSemaphoreGive(g_i2c_mutex);
    }

    if (isTouched && !wasTouched && (now - lastTouchMs > 200)) {
      lastTouchMs = now;
      wasTouched = true;
      touchX = x;
      touchY = y;

      Serial.printf("[TOUCH] Pressed at X:%d, Y:%d\n", touchX, touchY);

      // Handle Bottom Navigation Tab Taps (y >= 280)
      if (touchY >= 280) {
        if (touchX < 47 && currentUiPage != PAGE_RIDE) {
          Serial.println("[UI] Switch to RIDE page");
          currentUiPage = PAGE_RIDE;
          forceRedraw = true;
        } else if (touchX >= 47 && touchX < 93 && currentUiPage != PAGE_CLIMB) {
          Serial.println("[UI] Switch to CLIMB page");
          currentUiPage = PAGE_CLIMB;
          forceRedraw = true;
        } else if (touchX >= 93 && touchX < 139 && currentUiPage != PAGE_GPS_INFO) {
          Serial.println("[UI] Switch to SENSORS page");
          currentUiPage = PAGE_GPS_INFO;
          forceRedraw = true;
        } else if (touchX >= 139 && touchX < 185 && currentUiPage != PAGE_DEBUG) {
          Serial.println("[UI] Switch to NMEA DEBUG page");
          currentUiPage = PAGE_DEBUG;
          forceRedraw = true;
        } else if (touchX >= 185 && currentUiPage != PAGE_SETTINGS) {
          Serial.println("[UI] Switch to SETTINGS page");
          currentUiPage = PAGE_SETTINGS;
          forceRedraw = true;
        }
      } else {
        // Page-specific touch handlers
        if (currentUiPage == PAGE_RIDE) {
          if (handleRidePageTouch(touchX, touchY)) {
            Serial.println("[UI] Ride state toggled via touch");
            forceRedraw = true;
          }
        } else if (currentUiPage == PAGE_GPS_INFO) {
          if (handleSensorsPageTouch(touchX, touchY)) {
            Serial.println("[UI] Sensor page action triggered");
            forceRedraw = true;
          }
        } else if (currentUiPage == PAGE_SETTINGS) {
          if (handleSettingsPageTouch(touchX, touchY)) {
            Serial.println("[UI] Setting updated via touch");
            forceRedraw = true;
          }
        }
      }
    } else if (!isTouched) {
      wasTouched = false;
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
      renderNavBar();
      forceRedraw = false;
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // ~20 FPS rendering loop
  }
}
