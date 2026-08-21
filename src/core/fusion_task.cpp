#include "fusion_task.h"
#include "hardware/gps_task.h"
#include "hardware/baro_task.h"
#include "hardware/battery.h"
#include <math.h>

static double haversineDistanceKm(double lat1, double lon1, double lat2, double lon2) {
  if (lat1 == 0.0 || lon1 == 0.0 || lat2 == 0.0 || lon2 == 0.0) return 0.0;
  double dLat = (lat2 - lat1) * M_PI / 180.0;
  double dLon = (lon2 - lon1) * M_PI / 180.0;
  lat1 = lat1 * M_PI / 180.0;
  lat2 = lat2 * M_PI / 180.0;

  double a = pow(sin(dLat / 2.0), 2) + pow(sin(dLon / 2.0), 2) * cos(lat1) * cos(lat2);
  double c = 2.0 * asin(sqrt(a));
  return 6371.0 * c;
}

void startFusionTask() {
  xTaskCreatePinnedToCore(
    fusionTaskLoop,
    "FusionTask",
    4096,
    NULL,
    3, // Priority 3
    NULL,
    0  // Core 0
  );
}

void fusionTaskLoop(void* pvParameters) {
  initBatteryADC();
  GpsFix fix;
  BaroSample baro;
  TelemetryState state = getTelemetrySnapshot();

  uint32_t speedAbove3StartMs = 0;
  uint32_t speedZeroStartMs = 0;
  uint32_t lastSecondTickMs = millis();
  uint32_t lastBatCheckMs = 0;
  double prevLat = 0.0;
  double prevLon = 0.0;
  float prevAlt = 0.0f;
  float smoothAlt = 0.0f;
  float distForGradeKm = 0.0f;

  for (;;) {
    uint32_t now = millis();

    if (now - lastBatCheckMs >= 2000) {
      lastBatCheckMs = now;
      state.battery_pct = readBatteryPercentage();
    }
    bool gotGpsFix = false;
    if (g_gps_queue != NULL && xQueueReceive(g_gps_queue, &fix, pdMS_TO_TICKS(50)) == pdTRUE) {
      gotGpsFix = true;
    }

    bool gotBaroSample = false;
    if (g_baro_queue != NULL && xQueueReceive(g_baro_queue, &baro, 0) == pdTRUE) {
      gotBaroSample = true;
    }

    if (gotBaroSample && baro.isValid) {
      if (smoothAlt == 0.0f) {
        smoothAlt = baro.altitudeM;
      } else {
        smoothAlt = (smoothAlt * 0.85f) + (baro.altitudeM * 0.15f); // Low pass filter
      }

      state.altitude_m = smoothAlt;

      if (prevAlt != 0.0f && state.ride_state == RIDE_STATE_ACTIVE) {
        float altDiff = smoothAlt - prevAlt;
        if (altDiff > 0.5f) { // Accumulate ascent above noise threshold
          state.total_ascent_m += altDiff;
          prevAlt = smoothAlt;
        } else if (altDiff < -0.5f) {
          prevAlt = smoothAlt;
        }
      } else if (prevAlt == 0.0f) {
        prevAlt = smoothAlt;
      }
    }

    if (gotGpsFix) {
      state.gps_has_fix = fix.isValid;
      state.satellites = fix.satellites;
      state.hdop = fix.hdop;
      state.gps_fix_quality = fix.isValid ? 1 : 0;

      if (fix.isValid) {
        state.lat = fix.latitude;
        state.lon = fix.longitude;
        if (!gotBaroSample || !baro.isValid) {
          state.altitude_m = fix.altitudeM;
        }

        if (state.speed_source != SPEED_SOURCE_BLE_CSC) {
          state.speed_kmh = fix.speedKmh;
          state.speed_source = SPEED_SOURCE_GPS;
        }

        if (prevLat != 0.0 && prevLon != 0.0 && state.ride_state == RIDE_STATE_ACTIVE) {
          double dist = haversineDistanceKm(prevLat, prevLon, fix.latitude, fix.longitude);
          if (dist > 0.001 && dist < 0.1) {
            state.trip_distance_km += (float)dist;
            distForGradeKm += (float)dist;

            // Grade % calculation over ~50m intervals
            if (distForGradeKm >= 0.05f) {
              float dAlt = smoothAlt - prevAlt;
              float dDistM = distForGradeKm * 1000.0f;
              state.grade_pct = (dAlt / dDistM) * 100.0f;
              distForGradeKm = 0.0f;
            }
          }
        }
        prevLat = fix.latitude;
        prevLon = fix.longitude;
      } else {
        if (state.speed_source == SPEED_SOURCE_GPS) {
          state.speed_kmh = 0.0f;
        }
      }
    }

    // Ride Auto Start/Stop State Machine
    float currentSpeed = state.speed_kmh;

    if (state.ride_state == RIDE_STATE_IDLE) {
      if (currentSpeed > 3.0f && state.gps_has_fix) {
        if (speedAbove3StartMs == 0) speedAbove3StartMs = now;
        if (now - speedAbove3StartMs >= 5000) {
          state.ride_state = RIDE_STATE_ACTIVE;
          speedAbove3StartMs = 0;
        }
      } else {
        speedAbove3StartMs = 0;
      }
    } else if (state.ride_state == RIDE_STATE_ACTIVE) {
      if (currentSpeed > state.max_speed_kmh) {
        state.max_speed_kmh = currentSpeed;
      }

      if (currentSpeed < 1.0f) {
        if (speedZeroStartMs == 0) speedZeroStartMs = now;
        if (now - speedZeroStartMs >= 30000) {
          state.ride_state = RIDE_STATE_PAUSED;
          speedZeroStartMs = 0;
        }
      } else {
        speedZeroStartMs = 0;
      }
    } else if (state.ride_state == RIDE_STATE_PAUSED) {
      if (currentSpeed > 3.0f) {
        state.ride_state = RIDE_STATE_ACTIVE;
      }
    }

    // 1Hz Ride Time and Average Speed updates
    if (now - lastSecondTickMs >= 1000) {
      lastSecondTickMs = now;
      if (state.ride_state == RIDE_STATE_ACTIVE) {
        state.ride_time_s++;
        if (state.ride_time_s > 0) {
          state.avg_speed_kmh = (state.trip_distance_km / (float)state.ride_time_s) * 3600.0f;
        }
      }
    }

    setTelemetryState(state);
  }
}
