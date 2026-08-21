#include "logger_task.h"
#include "gpx_writer.h"
#include "config/pins.h"
#include "core/telemetry_state.h"

static GpxWriter gpxWriter;

void startLoggerTask() {
  xTaskCreatePinnedToCore(
    loggerTaskLoop,
    "LoggerTask",
    4096,
    NULL,
    1, // Priority 1
    NULL,
    0  // Core 0
  );
}

void loggerTaskLoop(void* pvParameters) {
  bool sdMounted = false;

  // Configure SDIO pins for SD_MMC
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0, PIN_SD_D1, PIN_SD_D2, PIN_SD_D3);

  if (SD_MMC.begin("/sdcard", false /* 4-bit mode */, false /* format_if_mount_failed */)) {
    sdMounted = true;
    uint8_t cardType = SD_MMC.cardType();
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("[SD LOG] microSD card mounted successfully! Type: %d, Size: %llu MB\n", cardType, cardSize);
  } else {
    Serial.println("[SD LOG WARNING] microSD card not inserted or failed to mount via SDIO.");
  }

  // Update SD status in global telemetry state
  TelemetryState state = getTelemetrySnapshot();
  state.sd_status = sdMounted;
  setTelemetryState(state);

  RideState prevRideState = RIDE_STATE_IDLE;
  uint32_t lastLogMs = 0;

  for (;;) {
    state = getTelemetrySnapshot();

    // Handle Ride State Transitions
    if (prevRideState == RIDE_STATE_IDLE && state.ride_state == RIDE_STATE_ACTIVE) {
      if (sdMounted) {
        gpxWriter.openNewRideFile(2026, 8, 21, 12, 0, 0); // Timestamped from GPS UTC when available
      }
    } else if (prevRideState == RIDE_STATE_ACTIVE && state.ride_state == RIDE_STATE_IDLE) {
      if (sdMounted && gpxWriter.isOpen()) {
        gpxWriter.closeRideFile();
      }
    }
    prevRideState = state.ride_state;

    // 1Hz Trackpoint appending while ride is ACTIVE
    uint32_t now = millis();
    if (state.ride_state == RIDE_STATE_ACTIVE && (now - lastLogMs >= 1000)) {
      lastLogMs = now;
      if (sdMounted && gpxWriter.isOpen()) {
        gpxWriter.appendTrackPoint(state, 2026, 8, 21, 12, 0, 0);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}
