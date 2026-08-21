#include "settings.h"

Settings g_settings;
static Preferences prefs;

void initSettings() {
  prefs.begin("opencyclo", false);
  g_settings.units = prefs.getUChar("units", 0);
  g_settings.brightness = prefs.getUChar("bright", 200);
  g_settings.wheel_circumference_mm = prefs.getUShort("wheel", 2096);
  g_settings.sd_logging_enabled = prefs.getBool("sdlog", true);
  prefs.end();

  Serial.printf("[SETTINGS] Loaded NVS preferences: Units=%u, Brightness=%u, Wheel=%u mm, SDLog=%d\n",
                g_settings.units, g_settings.brightness, g_settings.wheel_circumference_mm, g_settings.sd_logging_enabled);
}

void saveSettings() {
  prefs.begin("opencyclo", false);
  prefs.putUChar("units", g_settings.units);
  prefs.putUChar("bright", g_settings.brightness);
  prefs.putUShort("wheel", g_settings.wheel_circumference_mm);
  prefs.putBool("sdlog", g_settings.sd_logging_enabled);
  prefs.end();

  Serial.println("[SETTINGS] Saved NVS preferences!");
}
