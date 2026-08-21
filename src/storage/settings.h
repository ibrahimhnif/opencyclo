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
};

extern Settings g_settings;

void initSettings();
void saveSettings();

#endif // OPENCYCLO_STORAGE_SETTINGS_H
