#include "logger_task.h"
#include "gpx_writer.h"
#include "ride_log_session.h"
#include "config/pins.h"
#include "core/telemetry_state.h"
#include "storage/settings.h"
#include <atomic>
#include <cmath>
#include "sd_access.h"
#include "sd_mount_policy.h"
#include "gps_diagnostics.h"
#include "gps_cache.h"

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
    // SD/FAT plus float CSV formatting need more headroom with diagnostics.
    GPS_DIAGNOSTICS_ENABLED ? 12288 : 4096,
    NULL,
    1, // Priority 1
    NULL,
    0  // Core 0
  );
}

void loggerTaskLoop(void* pvParameters) {
  bool sdMounted = false;
  SdMountPolicy mountPolicy;

  // Update SD status in global telemetry state
  TelemetryState state = getTelemetrySnapshot();
  state.sd_status = sdMounted;
  setSdStatus(sdMounted);

  uint32_t lastLogMs = 0;

  for (;;) {
    {
    SdGuard sd;
    if (!sd.locked) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
    if (stopState.load() != 0) {
      closeGpsDiagnostics();
      if (!gpxWriter.isOpen() || gpxWriter.closeRideFile()) {
        int expected = 1;
        stopState.compare_exchange_strong(expected, 2);
      }
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    state = getTelemetrySnapshot();

    if(mountPolicy.tick(millis(),gpxWriter.isOpen() || state.ride_save==RIDE_SAVE_ERROR,
      [](bool oneBit,int khz) {
        // Called only while holding the shared SD lock, before any file opens.
        SD_MMC.end();
        if(!SD_MMC.setPins(PIN_SD_CLK,PIN_SD_CMD,PIN_SD_D0,PIN_SD_D1,PIN_SD_D2,PIN_SD_D3)) {
          Serial.println("[SD MOUNT] pin configuration failed");return false;
        }
        Serial.printf("[SD MOUNT] trying %u-bit %d kHz; CLK=%d CMD=%d D0=%d D1=%d D2=%d D3=%d; format=OFF\n",
          oneBit?1:4,khz,PIN_SD_CLK,PIN_SD_CMD,PIN_SD_D0,PIN_SD_D1,PIN_SD_D2,PIN_SD_D3);
        if(!SD_MMC.begin("/sdcard",oneBit,false,khz) || SD_MMC.cardType()==CARD_NONE) {
          SD_MMC.end();Serial.println("[SD MOUNT] failed; retry in 5s. Check card, wiring and supply; no format performed.");
          return false;
        }
        Serial.printf("[SD MOUNT] OK %u-bit %d kHz, size=%llu MB; maps=%d routes=%d\n",
          oneBit?1:4,khz,SD_MMC.cardSize()/(1024*1024),SD_MMC.exists("/maps"),SD_MMC.exists("/routes"));
        return true;
      })) {
      sdMounted=true;g_sd_ready=true;
      SD_MMC.mkdir("/routes");SD_MMC.mkdir("/maps");
      state.sd_status=true;setSdStatus(true);
    }

    if(sdMounted)drainGpsDiagnostics(state,gpxWriter.isOpen()?gpxWriter.getFilename():"");
    // Cache filesystem work is owned by the logger and never runs in BLE/GPS callbacks.
    if(state.ride_state==RIDE_STATE_IDLE)gpsCacheStorageTick(sdMounted);

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
