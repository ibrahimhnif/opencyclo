#include "fusion_task.h"
#include "hardware/gps_task.h"
#include "hardware/baro_task.h"
#include "hardware/battery.h"
#include <math.h>
#include "display_speed.h"
#include "ride_speed_validity.h"
#include "altitude_policy.h"
#include "hardware/baro_calibration.h"

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
  GpsFix fix{};
  BaroSample baro{};
  DisplaySpeed displaySpeed;
  AltitudeCalibrationWindow calibrationWindow;
  GpsAltitudeFilter gpsAltitude;
  float lastReference=getBaroReference();
  uint32_t lastBaro=0;
  bool haveBaro=false,haveAltitude=false;

  uint32_t speedAbove4StartMs = 0;
  uint32_t speedBelow1_5StartMs = 0;
  uint32_t lastSecondTickMs = millis();
  uint32_t lastBatCheckMs = 0;
  double prevLat = 0.0;
  double prevLon = 0.0;
  float prevAlt = 0.0f;
  float smoothAlt = 0.0f;
  float distForGradeKm = 0.0f;
  uint32_t rideRevision = 0;

  for (;;) {
    uint32_t now = millis();
    expireCscTelemetry(now);

    // Re-fetch a fresh snapshot every iteration rather than reusing one
    // persistent local copy across the whole task lifetime. FusionTask
    // doesn't own every field in TelemetryState (heart_rate_bpm/
    // cadence_rpm/power_watts are BleTask's) but it writes back the WHOLE
    // struct every ~50ms regardless -- with a stale one-time snapshot, that
    // meant FusionTask was silently stomping BleTask's live BLE sensor
    // updates back to whatever they were when this task started (typically
    // -1, since it starts before any sensor connects), every single cycle.
    // Symptom: the heart rate tile flickered between a real reading and 0
    // as the two tasks' writes raced. Fields FusionTask itself computes
    // (trip_distance_km, max_speed_kmh, ride_state, etc.) are unaffected --
    // they round-trip through the same shared state either way, since
    // FusionTask remains their only writer.
    TelemetryState state = getTelemetrySnapshot();
    if (rideRevision != state.ride_revision || state.ride_state != RIDE_STATE_ACTIVE) {
      prevLat = prevLon = 0;distForGradeKm = 0;prevAlt = state.altitude_m;
      if (rideRevision != state.ride_revision) {
        speedAbove4StartMs = speedBelow1_5StartMs = 0;lastSecondTickMs = now;
      }
      rideRevision = state.ride_revision;
    }

    // Check battery every 2 seconds
    if (now - lastBatCheckMs >= 2000) {
      lastBatCheckMs = now;
      state.battery_pct = readBatteryPercentage();
    }

    bool gotGpsFix = false;
    if (g_gps_queue != NULL && xQueueReceive(g_gps_queue, &fix, pdMS_TO_TICKS(50)) == pdTRUE) {
      gotGpsFix = true;
    }
    now=millis();
    if (now-fix.receivedAtMs>=1500) {
      fix.isValid=false;fix.speedValid=false;fix.quality=0;gotGpsFix=true;
    }

    bool gotBaroSample = false;
    if (g_baro_queue != NULL && xQueueReceive(g_baro_queue, &baro, 0) == pdTRUE) {
      gotBaroSample = true;
    }

    // Reset the smoothing baseline immediately after an idle calibration, so
    // its settling tail cannot become artificial ascent at the next start.
    const bool referenceChanged=gotBaroSample && baro.isValid && baro.referenceHpa!=lastReference;
    if(referenceChanged){haveAltitude=false;distForGradeKm=0;lastReference=baro.referenceHpa;}
    // Smooth Altitude with EMA filter
    if (gotBaroSample && baro.isValid) {
      lastBaro=now;haveBaro=true;
      state.baro_pressure_hpa=baro.pressureHpa;
      state.baro_temperature_c=baro.temperatureC;
      if (!haveAltitude || !state.baro_valid) {
        smoothAlt = baro.altitudeM;
      } else {
        smoothAlt = 0.85f * smoothAlt + 0.15f * baro.altitudeM;
      }
      state.altitude_m = smoothAlt;
      haveAltitude=true;
    }
    state.baro_age_ms=haveBaro?now-lastBaro:UINT32_MAX;
    state.baro_valid=haveBaro && state.baro_age_ms<2000 && (!gotBaroSample || baro.isValid);
    uint8_t oldAltitudeSource=state.altitude_source;
    const bool gpsAltitudeUsable=fix.isValid && fix.altitudeValid && std::isfinite(fix.altitudeM) &&
      (!fix.accuracyValid || (std::isfinite(fix.verticalAccuracyM) && fix.verticalAccuracyM>0 && fix.verticalAccuracyM<=25));
    float filteredGpsAltitude=gpsAltitude.update(fix.receivedAtMs,gpsAltitudeUsable,fix.altitudeM);
    int calibrationElevation=0;
    const bool canAuto=getBaroAutoEnabled() && !baroAutoDone() && state.ride_state==RIDE_STATE_IDLE &&
      state.baro_valid && gpsAltitudeUsable && fix.accuracyValid && fix.verticalAccuracyM<=10 &&
      fix.speedValid && fix.speedKmh<1.5f && state.speed_kmh<1.5f;
    if(calibrationWindow.update(fix.receivedAtMs,canAuto,fix.altitudeM,calibrationElevation))
      requestBaroAutoCalibration(calibrationElevation);
    state.altitude_valid=state.baro_valid || gpsAltitudeUsable;
    state.altitude_source=state.baro_valid?1:state.altitude_valid?2:0;
    if(state.altitude_source==2)state.altitude_m=filteredGpsAltitude;
    if(!state.altitude_valid) {state.altitude_m=0;state.grade_pct=0;haveAltitude=false;}
    if(referenceChanged || oldAltitudeSource!=state.altitude_source) {prevAlt=state.altitude_m;distForGradeKm=0;}

    // Process GPS Telemetry Fix
    if (gotGpsFix) {
      state.gps_year=fix.year;state.gps_month=fix.month;state.gps_day=fix.day;
      state.gps_hour=fix.hour;state.gps_minute=fix.minute;state.gps_second=fix.second;
      state.gps_has_fix = fix.isValid;
      state.gps_fix_quality = fix.quality;
      state.satellites = fix.satellites;
      state.hdop = fix.hdop;
      if (!fix.isValid || !fix.speedValid) {
        if (state.speed_source != SPEED_SOURCE_BLE_CSC) {
          state.speed_kmh=0;state.speed_source=SPEED_SOURCE_NONE;
        }
        prevLat=prevLon=0;
      }
      if(fix.speedKmh==0)prevLat=prevLon=0;

      if (fix.isValid) {
        state.lat = fix.latitude;
        state.lon = fix.longitude;

        // If no BLE CSC speed sensor is connected, use GPS speed
        if (fix.speedValid && state.speed_source != SPEED_SOURCE_BLE_CSC) {
          state.speed_kmh = fix.speedKmh;
          state.speed_source = SPEED_SOURCE_GPS;
        }

        // Accumulate trip distance when ride is ACTIVE
        if (state.ride_state == RIDE_STATE_ACTIVE && fix.speedValid && fix.speedKmh>0) {
          if (prevLat != 0.0 && prevLon != 0.0) {
            double deltaKm = haversineDistanceKm(prevLat, prevLon, fix.latitude, fix.longitude);
            // Ignore unrealistic teleports (> 150 km/h equivalent per sample)
            if (deltaKm > 0.0005 && deltaKm < 0.05) {
              state.trip_distance_km += (float)deltaKm;
              distForGradeKm += (float)deltaKm;

              // Calculate Grade % every 50 meters
              if (distForGradeKm >= 0.05f) {
                float dAlt = state.altitude_valid?state.altitude_m - prevAlt:0;
                if (dAlt>0) {
                  state.total_ascent_m += dAlt;
                }
                state.grade_pct = (dAlt / (distForGradeKm * 1000.0f)) * 100.0f;
                // Clamp grade between -30% and +30%
                if (state.grade_pct < -30.0f) state.grade_pct = -30.0f;
                if (state.grade_pct > 30.0f) state.grade_pct = 30.0f;

                distForGradeKm = 0.0f;
                prevAlt = state.altitude_m;
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

    // Movement only pauses/resumes an explicitly started session.
    float effectiveSpeed = state.speed_kmh;

    if (!state.ride_auto_allowed || state.ride_state == RIDE_STATE_IDLE ||
        !rideSpeedIsValid(state,fix,now)) {
      speedAbove4StartMs = speedBelow1_5StartMs = 0;
    } else if (effectiveSpeed >= 4.0f) {
      speedBelow1_5StartMs = 0;
      if (speedAbove4StartMs == 0) {
        speedAbove4StartMs = now;
      } else if (now - speedAbove4StartMs >= 3000) { // Speed >= 4.0 km/h for 3 continuous seconds
        if (state.ride_state == RIDE_STATE_PAUSED) {
          state.ride_state = RIDE_STATE_ACTIVE;
          Serial.println("[STATE MACHINE] Auto-resumed ride. Speed >= 4.0 km/h for 3s.");
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

    state.display_speed_kmh=displaySpeed.update(state.speed_kmh,state.speed_source,now);
    setFusionTelemetryState(state);

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
