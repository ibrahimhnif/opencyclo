#include "telemetry_state.h"

TelemetryState g_telemetry;
GpsDebugInfo g_gps_debug;
SemaphoreHandle_t g_telemetry_mutex = NULL;
SemaphoreHandle_t g_i2c_mutex = NULL;

void initTelemetryState() {
  if (g_telemetry_mutex == NULL) {
    g_telemetry_mutex = xSemaphoreCreateMutex();
  }
  if (g_i2c_mutex == NULL) {
    g_i2c_mutex = xSemaphoreCreateMutex();
  }

  if (xSemaphoreTake(g_telemetry_mutex, portMAX_DELAY) == pdTRUE) {
    g_telemetry.speed_kmh = 0.0f;
    g_telemetry.speed_source = SPEED_SOURCE_NONE;
    g_telemetry.avg_speed_kmh = 0.0f;
    g_telemetry.max_speed_kmh = 0.0f;
    g_telemetry.trip_distance_km = 0.0f;
    g_telemetry.ride_time_s = 0;

    g_telemetry.cadence_rpm = -1;
    g_telemetry.heart_rate_bpm = -1;
    g_telemetry.power_watts = -1;

    g_telemetry.lat = 0.0;
    g_telemetry.lon = 0.0;
    g_telemetry.altitude_m = 0.0f;
    g_telemetry.grade_pct = 0.0f;
    g_telemetry.total_ascent_m = 0.0f;
    g_telemetry.heading_deg = 0.0f;
    g_telemetry.heading_valid = false;
    g_telemetry.heading_source = 0;

    g_telemetry.gps_has_fix = false;
    g_telemetry.gps_fix_quality = 0;
    g_telemetry.satellites = 0;
    g_telemetry.hdop = 99.99f;

    g_telemetry.ble_connection_status[0] = 0;
    g_telemetry.ble_connection_status[1] = 0;
    g_telemetry.ble_connection_status[2] = 0;
    g_telemetry.sd_status = false;
    g_telemetry.battery_pct = 100;

    g_telemetry.ride_state = RIDE_STATE_IDLE;
    g_telemetry.ride_revision = 0;
    g_telemetry.ride_auto_allowed = false;
    g_telemetry.ride_save = RIDE_SAVE_NONE;
    g_telemetry.ride_file[0] = 0;
    g_telemetry.gps_year = 0;
    g_telemetry.gps_month = g_telemetry.gps_day = g_telemetry.gps_hour = g_telemetry.gps_minute = g_telemetry.gps_second = 0;

    g_gps_debug.total_chars = 0;
    g_gps_debug.sentences_passed = 0;
    g_gps_debug.active_rx_pin = 44;
    g_gps_debug.line_head = 0;
    for (int i = 0; i < NMEA_BUFFER_LINES; i++) {
      g_gps_debug.last_lines[i][0] = '\0';
    }

    xSemaphoreGive(g_telemetry_mutex);
  }
}

TelemetryState getTelemetrySnapshot() {
  TelemetryState snap{};
  if (xSemaphoreTake(g_telemetry_mutex, portMAX_DELAY) == pdTRUE) {
    snap = g_telemetry;
    xSemaphoreGive(g_telemetry_mutex);
  }
  return snap;
}

static void mergeTelemetryState(const TelemetryState& newState, bool fusion) {
  if (xSemaphoreTake(g_telemetry_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    TelemetryState next = newState;
    next.csc_sample_at_ms=g_telemetry.csc_sample_at_ms;
    next.cadence_rpm=g_telemetry.cadence_rpm;
    next.ble_connection_status[0]=g_telemetry.ble_connection_status[0];
    next.sd_status=g_telemetry.sd_status;
    // BLE callbacks must not restore a GPS snapshot taken before fusion
    // invalidated it. GPS owns these fields; CSC still owns wheel speed.
    if (!fusion) {
      next.display_speed_kmh=g_telemetry.display_speed_kmh;
      next.altitude_m=g_telemetry.altitude_m;next.altitude_valid=g_telemetry.altitude_valid;
      next.altitude_source=g_telemetry.altitude_source;next.grade_pct=g_telemetry.grade_pct;
      next.baro_valid=g_telemetry.baro_valid;next.baro_pressure_hpa=g_telemetry.baro_pressure_hpa;
      next.baro_temperature_c=g_telemetry.baro_temperature_c;next.baro_age_ms=g_telemetry.baro_age_ms;
      next.heading_deg=g_telemetry.heading_deg;next.heading_valid=g_telemetry.heading_valid;
      next.heading_source=g_telemetry.heading_source;
      next.gps_has_fix=g_telemetry.gps_has_fix;
      next.gps_fix_quality=g_telemetry.gps_fix_quality;
      next.lat=g_telemetry.lat;next.lon=g_telemetry.lon;
      next.satellites=g_telemetry.satellites;next.hdop=g_telemetry.hdop;
      next.gps_year=g_telemetry.gps_year;next.gps_month=g_telemetry.gps_month;
      next.gps_day=g_telemetry.gps_day;next.gps_hour=g_telemetry.gps_hour;
      next.gps_minute=g_telemetry.gps_minute;next.gps_second=g_telemetry.gps_second;
      next.speed_source=g_telemetry.speed_source;next.speed_kmh=g_telemetry.speed_kmh;
    } else if(g_telemetry.speed_source==SPEED_SOURCE_BLE_CSC || next.speed_source==SPEED_SOURCE_BLE_CSC) {
      next.speed_source=g_telemetry.speed_source;next.speed_kmh=g_telemetry.speed_kmh;
    }
    if(fusion) {
      next.cadence_rpm=g_telemetry.cadence_rpm;next.heart_rate_bpm=g_telemetry.heart_rate_bpm;
      next.power_watts=g_telemetry.power_watts;
      memcpy(next.ble_connection_status,g_telemetry.ble_connection_status,sizeof(next.ble_connection_status));
      next.sd_status=g_telemetry.sd_status;
    }
    // Commands own lifecycle; only fusion owns accumulated ride statistics.
    // Reject a fusion snapshot taken before a pause/finish/new ride command.
    if (!fusion || next.ride_revision != g_telemetry.ride_revision ||
        g_telemetry.ride_save != RIDE_SAVE_NONE ||
        g_telemetry.ride_state == RIDE_STATE_IDLE) {
      next.ride_state = g_telemetry.ride_state;
      next.trip_distance_km = g_telemetry.trip_distance_km;
      next.ride_time_s = g_telemetry.ride_time_s;
      next.avg_speed_kmh = g_telemetry.avg_speed_kmh;
      next.max_speed_kmh = g_telemetry.max_speed_kmh;
      next.total_ascent_m = g_telemetry.total_ascent_m;
    }
    if (!g_telemetry.ride_auto_allowed) next.ride_state = g_telemetry.ride_state;
    next.ride_revision = g_telemetry.ride_revision;
    next.ride_auto_allowed = g_telemetry.ride_auto_allowed;
    next.ride_save = g_telemetry.ride_save;
    memcpy(next.ride_file,g_telemetry.ride_file,sizeof(next.ride_file));
    g_telemetry = next;
    xSemaphoreGive(g_telemetry_mutex);
  }
}
void setTelemetryState(const TelemetryState& state) { mergeTelemetryState(state,false); }
void setFusionTelemetryState(const TelemetryState& state) { mergeTelemetryState(state,true); }
void setSdStatus(bool ready) {
  if(xSemaphoreTake(g_telemetry_mutex,portMAX_DELAY)!=pdTRUE)return;
  g_telemetry.sd_status=ready;xSemaphoreGive(g_telemetry_mutex);
}
void setCscTelemetry(float speed,int16_t cadence,uint8_t connected) {
  if(xSemaphoreTake(g_telemetry_mutex,portMAX_DELAY)!=pdTRUE)return;
  g_telemetry.cadence_rpm=cadence;
  g_telemetry.csc_sample_at_ms=millis();
  g_telemetry.ble_connection_status[0]=connected?2:0;
  if(speed>=0){g_telemetry.speed_kmh=speed;g_telemetry.speed_source=SPEED_SOURCE_BLE_CSC;}
  else if(g_telemetry.speed_source==SPEED_SOURCE_BLE_CSC){g_telemetry.speed_kmh=0;g_telemetry.speed_source=SPEED_SOURCE_NONE;}
  xSemaphoreGive(g_telemetry_mutex);
}
void expireCscTelemetry(uint32_t now) {
  if(xSemaphoreTake(g_telemetry_mutex,portMAX_DELAY)!=pdTRUE)return;
  if(now-g_telemetry.csc_sample_at_ms>5000) {
    if(g_telemetry.speed_source==SPEED_SOURCE_BLE_CSC){g_telemetry.speed_source=SPEED_SOURCE_NONE;g_telemetry.speed_kmh=0;}
    g_telemetry.cadence_rpm=-1;
  }
  xSemaphoreGive(g_telemetry_mutex);
}

bool setManualRideState(RideState state) {
  if (state != RIDE_STATE_ACTIVE && state != RIDE_STATE_PAUSED) return false;
  if (xSemaphoreTake(g_telemetry_mutex, pdMS_TO_TICKS(50)) != pdTRUE) return false;
  bool ok = g_telemetry.ride_save != RIDE_SAVE_PENDING && g_telemetry.ride_save != RIDE_SAVE_ERROR &&
            !(state == RIDE_STATE_PAUSED && g_telemetry.ride_state == RIDE_STATE_IDLE);
  if (ok) {
    if (g_telemetry.ride_state == RIDE_STATE_IDLE) {
      g_telemetry.trip_distance_km = g_telemetry.avg_speed_kmh = g_telemetry.max_speed_kmh = 0;
      g_telemetry.total_ascent_m = 0;g_telemetry.ride_time_s = 0;
      g_telemetry.ride_save = RIDE_SAVE_NONE;g_telemetry.ride_file[0] = 0;
    }
    g_telemetry.ride_state = state;
    g_telemetry.ride_auto_allowed = state == RIDE_STATE_ACTIVE;
    ++g_telemetry.ride_revision;
  }
  xSemaphoreGive(g_telemetry_mutex);return ok;
}
bool requestFinishRide() {
  if (xSemaphoreTake(g_telemetry_mutex, pdMS_TO_TICKS(50)) != pdTRUE) return false;
  bool ok = (g_telemetry.ride_state != RIDE_STATE_IDLE && g_telemetry.ride_save == RIDE_SAVE_NONE) ||
            g_telemetry.ride_save == RIDE_SAVE_ERROR;
  if (ok) {
    g_telemetry.ride_state = RIDE_STATE_IDLE;g_telemetry.ride_auto_allowed = false;
    g_telemetry.ride_save = RIDE_SAVE_PENDING;++g_telemetry.ride_revision;
  }
  xSemaphoreGive(g_telemetry_mutex);return ok;
}
void completeFinishRide(RideSaveState result,const char* filename) {
  if (result != RIDE_SAVE_OK && result != RIDE_SAVE_ERROR && result != RIDE_SAVE_NO_FILE) return;
  if (xSemaphoreTake(g_telemetry_mutex, portMAX_DELAY) == pdTRUE) {
    if (g_telemetry.ride_save == RIDE_SAVE_PENDING) {
      g_telemetry.ride_save = result;
      snprintf(g_telemetry.ride_file,sizeof(g_telemetry.ride_file),"%s",filename?filename:"");
    }
    xSemaphoreGive(g_telemetry_mutex);
  }
}

void addNmeaDebugLine(const char* line) {
  if (line == NULL || strlen(line) == 0) return;
  if (xSemaphoreTake(g_telemetry_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    uint8_t idx = g_gps_debug.line_head;
    snprintf(g_gps_debug.last_lines[idx], NMEA_LINE_MAX_LEN, "%s", line);
    g_gps_debug.line_head = (idx + 1) % NMEA_BUFFER_LINES;
    xSemaphoreGive(g_telemetry_mutex);
  }
}
