#include "ui_task.h"
#include "hardware/display.h"
#include "core/telemetry_state.h"
#include "engine/layout_manager.h"
#include "storage/layout_config.h"
#include "storage/settings.h"
#include "power_menu.h"
#include "ride_menu.h"
#include <stdlib.h>
#include "navigation/navigation.h"
#include "ui/frame_present.h"
#include "ui/gesture.h"
#include "ui/engine/resolved_theme.h"
#include "storage/theme_config.h"

static uint8_t currentPageIdx = 0;
static uint8_t activePageDrawn = 255;
static bool forceRedraw = true;

static bool powerOverlay(bool touched, int16_t x, int16_t y, uint32_t now) { return updatePowerUi(touched, x, y, now); }
static bool rideOverlay(bool touched, int16_t x, int16_t y, uint32_t) { return updateRideMenu(touched, x, y); }

// Full-screen overlays, highest priority first. `cancel` drops a pending press
// when a higher overlay takes the input; `delayMs` is that overlay's frame pace.
struct Overlay {
  bool (*update)(bool touched, int16_t x, int16_t y, uint32_t now);
  void (*cancel)();
  uint32_t delayMs;
};
static const Overlay kOverlays[] = {
  {powerOverlay, nullptr, 50},
  {rideOverlay, cancelRideMenuTouch, 10},
};

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
    } else {
      // A busy I2C bus is not a touch release (especially over Power Off).
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    // Overlays get input first, in order. When one consumes it, every touch
    // consumer below it is cancelled so a half-finished press can't fire later.
    bool overlayConsumed = false;
    for (size_t i = 0; i < sizeof(kOverlays) / sizeof(kOverlays[0]); i++) {
      if (!kOverlays[i].update(isTouched, x, y, now)) continue;
      cancelSensorWidgetTouch();
      cancelNavigationTouch();
      for (size_t j = i + 1; j < sizeof(kOverlays) / sizeof(kOverlays[0]); j++) {
        if (kOverlays[j].cancel) kOverlays[j].cancel();
      }
      wasTouched = false;
      touchStartX = touchStartY = -1;
      forceRedraw = true;
      vTaskDelay(pdMS_TO_TICKS(kOverlays[i].delayMs));
      overlayConsumed = true;
      break;
    }
    if (overlayConsumed) continue;

    int sensorSwipe=0;
    bool sensorTouch=!navigationOpen() && g_ui_config.active_page_count>0 &&
      handleSensorPageStream(g_ui_config.pages[currentPageIdx],isTouched,x,y,sensorSwipe);
    if(sensorTouch) {
      wasTouched=false;touchStartX=touchStartY=-1;
      if(sensorSwipe){
        int total=g_ui_config.active_page_count;
        currentPageIdx=(currentPageIdx+sensorSwipe+total)%total;
        cancelSensorWidgetTouch();forceRedraw=true;
      }
    } else if (navigationOpen()) {
      cancelSensorWidgetTouch();
      navigationTouch(isTouched,x,y);
      wasTouched=false;
      touchStartX=touchStartY=-1;
    } else if (isTouched) {
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

      if (now - lastTouchMs > ui::kGestureDebounceMs && touchStartX >= 0 && lastTouchX >= 0) {
        lastTouchMs = now;
        uint8_t totalPages = (g_ui_config.active_page_count > 0) ? g_ui_config.active_page_count : 1;
        // Map drags pan instead of changing dashboard pages.
        switch (ui::classifyRelease(lastTouchX - touchStartX, lastTouchY - touchStartY)) {
          case ui::Gesture::SwipeLeft:
            currentPageIdx = (currentPageIdx + 1) % totalPages;
            Serial.printf("[UI GESTURE] Swiped Left -> Page %u/%u\n", currentPageIdx + 1, totalPages);
            forceRedraw = true;
            break;
          case ui::Gesture::SwipeRight:
            currentPageIdx = (currentPageIdx + totalPages - 1) % totalPages;
            Serial.printf("[UI GESTURE] Swiped Right -> Page %u/%u\n", currentPageIdx + 1, totalPages);
            forceRedraw = true;
            break;
          case ui::Gesture::Tap:
            // Delegate to the current page layout & widgets.
            if (handlePageTouch(g_ui_config.pages[currentPageIdx], touchStartX, touchStartY)) forceRedraw = true;
            break;
          default:
            break;
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
    if (!navigationOpen()) updateNavigation(state);
    if (navigationOpen()) {
      renderNavigation(state);
      forceRedraw=true;
      // Poll touch between the map's 33 ms frame slots.
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    // Render active dynamic page from UiConfig -- every widget/layout draw
    // call targets the off-screen canvas sprite, not the panel directly.
    // A browser-pushed theme applies live: copy it between two reads of its
    // version counter and re-resolve only when they agree (no torn reads).
    {
      const uint32_t version = g_theme_version.load();
      const uitheme::UiTheme copy = g_theme;
      ui::refreshActiveTheme(copy, version, g_theme_version.load());
    }
    if (g_ui_config.active_page_count > 0) {
      renderPage(g_ui_config.pages[currentPageIdx], currentPageIdx, g_ui_config.active_page_count, state, forceRedraw);
    }

    // Blit the fully-composed frame to the panel in one SPI transfer. Doing
    // this every iteration (not just on forceRedraw) is what makes double
    // buffering actually work: the dynamic per-frame text updates inside
    // renderPage() above still only touch canvas, so without this the panel
    // would never see them. Measured at ~19-20ms on this hardware (240x320
    // RGB565, ~153KB) at the 80MHz SPI clock set in display.cpp.
    presentFrame();

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
