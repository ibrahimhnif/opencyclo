#ifndef OPENCYCLO_STORAGE_SETTINGS_H
#define OPENCYCLO_STORAGE_SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>

struct Settings {
  uint8_t units;                  // 0 = Metric (km/h, m), 1 = Imperial (mph, ft)
  uint8_t brightness;             // 10 .. 255
  uint16_t wheel_circumference_mm; // default 2096 mm
  bool sd_logging_enabled;
  char paired_csc_mac[18];
  char paired_hr_mac[18];
  char paired_power_mac[18];
  // BLE address type (0=PUBLIC, 1=RANDOM -- matches NimBLE's BLE_ADDR_*)
  // for paired_hr_mac. A MAC string alone isn't enough to reconnect:
  // NimBLEClient::connect() dials using this type, and most HR straps
  // advertise as RANDOM. Only recorded for HR since it's the only profile
  // that actually opens a GATT connection today (CSC/Power just remember a
  // MAC). Defaults to 0/PUBLIC, which matches pre-existing saved MACs from
  // before this field existed.
  uint8_t paired_hr_addr_type;

  // Insta360 wake-a-sleeping-camera beacon: bytes 14-19 of the reference
  // protocol's 26-byte manufacturer-data payload are unique to one physical
  // camera unit (read off its own Settings -> Camera Info screen) -- see
  // ble_camera_remote.cpp. All-zero means "not set yet."
  uint8_t insta360_wake_bytes[6];
};

extern Settings g_settings;

void initSettings();
void saveSettings();

#endif // OPENCYCLO_STORAGE_SETTINGS_H
