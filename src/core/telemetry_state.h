#ifndef OPENCYCLO_TELEMETRY_STATE_H
#define OPENCYCLO_TELEMETRY_STATE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define NMEA_BUFFER_LINES 8
#define NMEA_LINE_MAX_LEN 48

enum SpeedSource {
  SPEED_SOURCE_NONE = 0,
  SPEED_SOURCE_GPS,
  SPEED_SOURCE_BLE_CSC
};

enum RideState {
  RIDE_STATE_IDLE = 0,
  RIDE_STATE_ACTIVE,
  RIDE_STATE_PAUSED
};
enum RideSaveState { RIDE_SAVE_NONE, RIDE_SAVE_PENDING, RIDE_SAVE_OK, RIDE_SAVE_ERROR, RIDE_SAVE_NO_FILE };

struct GpsFix {
  bool isValid;
  double latitude;
  double longitude;
  float speedKmh;
  float altitudeM;
  float hdop;
  uint32_t satellites;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint32_t ageMs;
};

struct GpsDebugInfo {
  uint32_t total_chars;
  uint32_t sentences_passed;
  uint8_t active_rx_pin;
  char last_lines[NMEA_BUFFER_LINES][NMEA_LINE_MAX_LEN];
  uint8_t line_head;
};

struct TelemetryState {
  // Speed & Movement
  float speed_kmh;
  SpeedSource speed_source;
  float avg_speed_kmh;
  float max_speed_kmh;
  float trip_distance_km;
  uint32_t ride_time_s;

  // Sensors (BLE) - set to -1 when disconnected
  int16_t cadence_rpm;
  int16_t heart_rate_bpm;
  int16_t power_watts;

  // Navigation & Location
  double lat;
  double lon;
  float altitude_m;
  float grade_pct;
  float total_ascent_m;

  // GPS Status
  bool gps_has_fix;
  uint8_t gps_fix_quality;
  uint8_t satellites;
  float hdop;

  // System & Peripheral Status
  uint8_t ble_connection_status[3]; // [CSC, HR, POWER]
  bool sd_status;
  uint8_t battery_pct;
  
  // State Machine
  RideState ride_state;
  uint32_t ride_revision;
  bool ride_auto_allowed;
  RideSaveState ride_save;
  char ride_file[64];
  uint16_t gps_year;
  uint8_t gps_month, gps_day, gps_hour, gps_minute, gps_second;
};

extern TelemetryState g_telemetry;
extern GpsDebugInfo g_gps_debug;
extern SemaphoreHandle_t g_telemetry_mutex;
extern SemaphoreHandle_t g_i2c_mutex;

void initTelemetryState();
TelemetryState getTelemetrySnapshot();
void setTelemetryState(const TelemetryState& newState);
void setFusionTelemetryState(const TelemetryState& newState);
bool setManualRideState(RideState state);
bool requestFinishRide();
void completeFinishRide(RideSaveState result, const char* filename);
void addNmeaDebugLine(const char* line);

#endif // OPENCYCLO_TELEMETRY_STATE_H
