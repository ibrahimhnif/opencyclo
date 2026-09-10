#include "logger_task.h"
#include "gpx_writer.h"
#include "ride_log_session.h"
#include "config/pins.h"
#include "core/telemetry_state.h"
#include "storage/settings.h"
#include <atomic>
#include <cmath>
#include "sd_access.h"

static GpxWriter gpxWriter;
// 0 = running, 1 = stop requested, 2 = file closed and quiescent.
static std::atomic<int> stopState{0};

bool prepareLoggerForPowerOff(uint32_t timeoutMs) {
  stopState.store(1);
  uint32_t start = millis();
  while (stopState.load() != 2) {
    if (millis() - start >= timeoutMs) {
      stopState.store(0);
      return false;
    }
    delay(10);
  }
  return true;
}

void resumeLoggerAfterPowerOff() { stopState.store(0); }

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

  {
  SdGuard sd;

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
  g_sd_ready = sdMounted;
  if (sdMounted) { SD_MMC.mkdir("/routes"); SD_MMC.mkdir("/maps"); }
  }

  // Update SD status in global telemetry state
  TelemetryState state = getTelemetrySnapshot();
  state.sd_status = sdMounted;
  setTelemetryState(state);

  uint32_t lastLogMs = 0;

  for (;;) {
    {
    SdGuard sd;
    if (!sd.locked) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
    if (stopState.load() != 0) {
      if (!gpxWriter.isOpen() || gpxWriter.closeRideFile()) {
        int expected = 1;
        stopState.compare_exchange_strong(expected, 2);
      }
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    state = getTelemetrySnapshot();

    if (state.ride_save == RIDE_SAVE_PENDING) {
      auto result=finishRideLog(gpxWriter,sdMounted,g_settings.sd_logging_enabled,state);
      completeFinishRide(result,result==RIDE_SAVE_NO_FILE?"":gpxWriter.getFilename());
      continue;
    }
    // Retain a failed file for an explicit retry; do not silently close/reopen.
    if (state.ride_save == RIDE_SAVE_ERROR) {
      vTaskDelay(pdMS_TO_TICKS(20));continue;
    }

    // Keep paused rides open; close on stop, logging disabled, or power-off.
    if (g_settings.sd_logging_enabled && state.ride_state != RIDE_STATE_IDLE) {
      if (sdMounted && !gpxWriter.isOpen()) {
        gpxWriter.openNewRideFile(state.gps_year, state.gps_month, state.gps_day,
                                 state.gps_hour, state.gps_minute, state.gps_second);
      }
    } else {
      if (sdMounted && gpxWriter.isOpen()) {
        gpxWriter.closeRideFile();
      }
    }

    // 1Hz Trackpoint appending while ride is ACTIVE
    uint32_t now = millis();
    if (state.ride_state == RIDE_STATE_ACTIVE && (now - lastLogMs >= 1000)) {
      lastLogMs = now;
      if (sdMounted && gpxWriter.isOpen() && state.gps_has_fix &&
          std::isfinite(state.lat) && std::isfinite(state.lon) &&
          std::abs(state.lat)<=90 && std::abs(state.lon)<=180) {
        gpxWriter.appendTrackPoint(state, state.gps_year, state.gps_month, state.gps_day,
                                  state.gps_hour, state.gps_minute, state.gps_second);
      }
    }

    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}
