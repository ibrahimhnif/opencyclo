#include "fusion_task.h"
#include "hardware/gps_task.h"
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
  GpsFix fix;
  TelemetryState state = getTelemetrySnapshot();

  uint32_t speedAbove3StartMs = 0;
  uint32_t speedZeroStartMs = 0;
  uint32_t lastSecondTickMs = millis();
  double prevLat = 0.0;
  double prevLon = 0.0;

  for (;;) {
    bool gotGpsFix = false;
    if (g_gps_queue != NULL && xQueueReceive(g_gps_queue, &fix, pdMS_TO_TICKS(100)) == pdTRUE) {
      gotGpsFix = true;
    }

    uint32_t now = millis();

    if (gotGpsFix) {
      state.gps_has_fix = fix.isValid;
      state.satellites = fix.satellites;
      state.hdop = fix.hdop;
      state.gps_fix_quality = fix.isValid ? 1 : 0;

      if (fix.isValid) {
        state.lat = fix.latitude;
        state.lon = fix.longitude;
        state.altitude_m = fix.altitudeM;

        // Speed calculation priority (GPS default for now until BLE CSC added)
        if (state.speed_source != SPEED_SOURCE_BLE_CSC) {
          state.speed_kmh = fix.speedKmh;
          state.speed_source = SPEED_SOURCE_GPS;
        }

        // Distance accumulation
        if (prevLat != 0.0 && prevLon != 0.0 && state.ride_state == RIDE_STATE_ACTIVE) {
          double dist = haversineDistanceKm(prevLat, prevLon, fix.latitude, fix.longitude);
          if (dist > 0.001 && dist < 0.1) {
            state.trip_distance_km += (float)dist;
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
        if (now - speedAbove3StartMs >= 5000) { // Sustained > 3 km/h for 5s
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
        if (now - speedZeroStartMs >= 30000) { // Speed ~0 for 30s
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
