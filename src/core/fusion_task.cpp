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

  uint32_t speedAbove4StartMs = 0;
  uint32_t speedBelow1_5StartMs = 0;
  uint32_t lastSecondTickMs = millis();
  uint32_t lastBatCheckMs = 0;
  double prevLat = 0.0;
  double prevLon = 0.0;
  float prevAlt = 0.0f;
  float smoothAlt = 0.0f;
  float distForGradeKm = 0.0f;

  for (;;) {
    uint32_t now = millis();

    // Check battery every 2 seconds
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

    // Smooth Altitude with EMA filter
    if (gotBaroSample && baro.isValid) {
      if (smoothAlt == 0.0f) {
        smoothAlt = baro.altitudeM;
      } else {
        smoothAlt = 0.85f * smoothAlt + 0.15f * baro.altitudeM;
      }
      state.altitude_m = smoothAlt;
    }

    // Process GPS Telemetry Fix
    if (gotGpsFix) {
      state.gps_has_fix = fix.isValid;
      state.satellites = fix.satellites;
      state.hdop = fix.hdop;

      if (fix.isValid) {
        state.lat = fix.latitude;
        state.lon = fix.longitude;

        // If no BLE CSC speed sensor is connected, use GPS speed
        if (state.speed_source != SPEED_SOURCE_BLE_CSC) {
          state.speed_kmh = fix.speedKmh;
          state.speed_source = SPEED_SOURCE_GPS;
        }

        // Noise floor suppression: clamp GPS speed noise < 2.5 km/h to 0.0 km/h
        if (state.speed_source == SPEED_SOURCE_GPS && state.speed_kmh < 2.5f) {
          state.speed_kmh = 0.0f;
        }

        // Accumulate trip distance when ride is ACTIVE
        if (state.ride_state == RIDE_STATE_ACTIVE) {
          if (prevLat != 0.0 && prevLon != 0.0) {
            double deltaKm = haversineDistanceKm(prevLat, prevLon, fix.latitude, fix.longitude);
            // Ignore unrealistic teleports (> 150 km/h equivalent per sample)
            if (deltaKm > 0.0005 && deltaKm < 0.05) {
              state.trip_distance_km += (float)deltaKm;
              distForGradeKm += (float)deltaKm;

              // Calculate Grade % every 50 meters
              if (distForGradeKm >= 0.05f) {
                float dAlt = smoothAlt - prevAlt;
                if (smoothAlt > prevAlt) {
                  state.total_ascent_m += dAlt;
                }
                state.grade_pct = (dAlt / (distForGradeKm * 1000.0f)) * 100.0f;
                // Clamp grade between -30% and +30%
                if (state.grade_pct < -30.0f) state.grade_pct = -30.0f;
                if (state.grade_pct > 30.0f) state.grade_pct = 30.0f;

                distForGradeKm = 0.0f;
                prevAlt = smoothAlt;
              }
            }
          }
          prevLat = fix.latitude;
          prevLon = fix.longitude;

          // Track Max Speed
          if (state.speed_kmh > state.max_speed_kmh) {
            state.max_speed_kmh = state.speed_kmh;
          }
        }
      }
    }

    // Enforce 0.0 km/h when stationary
    if (state.speed_source == SPEED_SOURCE_GPS && state.speed_kmh < 2.5f) {
      state.speed_kmh = 0.0f;
    }

    // Movement Detection & Auto Start / Pause State Machine
    float effectiveSpeed = state.speed_kmh;

    if (effectiveSpeed >= 4.0f) {
      speedBelow1_5StartMs = 0;
      if (speedAbove4StartMs == 0) {
        speedAbove4StartMs = now;
      } else if (now - speedAbove4StartMs >= 3000) { // Speed >= 4.0 km/h for 3 continuous seconds
        if (state.ride_state != RIDE_STATE_ACTIVE) {
          state.ride_state = RIDE_STATE_ACTIVE;
          Serial.println("[STATE MACHINE] Auto-started ride! Speed >= 4.0 km/h for 3s.");
        }
      }
    } else if (effectiveSpeed < 1.5f) {
      speedAbove4StartMs = 0;
      if (state.ride_state == RIDE_STATE_ACTIVE) {
        if (speedBelow1_5StartMs == 0) {
          speedBelow1_5StartMs = now;
        } else if (now - speedBelow1_5StartMs >= 5000) { // Speed < 1.5 km/h for 5 continuous seconds
          state.ride_state = RIDE_STATE_PAUSED;
          Serial.println("[STATE MACHINE] Auto-paused ride. Speed < 1.5 km/h for 5s.");
        }
      }
    } else {
      // In hysteresis zone (1.5 km/h <= speed < 4.0 km/h)
      speedAbove4StartMs = 0;
      speedBelow1_5StartMs = 0;
    }

    // Increment Ride Timer (1 Hz tick) when ACTIVE
    if (now - lastSecondTickMs >= 1000) {
      lastSecondTickMs = now;
      if (state.ride_state == RIDE_STATE_ACTIVE) {
        state.ride_time_s++;
        if (state.ride_time_s > 0) {
          state.avg_speed_kmh = state.trip_distance_km / (state.ride_time_s / 3600.0f);
        }
      }
    }

    setTelemetryState(state);

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
