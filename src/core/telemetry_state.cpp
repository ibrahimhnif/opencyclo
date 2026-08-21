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
  TelemetryState snap;
  if (xSemaphoreTake(g_telemetry_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    snap = g_telemetry;
    xSemaphoreGive(g_telemetry_mutex);
  } else {
    snap = g_telemetry;
  }
  return snap;
}

void setTelemetryState(const TelemetryState& newState) {
  if (xSemaphoreTake(g_telemetry_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    g_telemetry = newState;
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
