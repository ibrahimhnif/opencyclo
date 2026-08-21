#include "settings.h"

Settings g_settings;
static Preferences prefs;

void initSettings() {
  prefs.begin("opencyclo", false);
  g_settings.units = prefs.getUChar("units", 0);
  g_settings.brightness = prefs.getUChar("bright", 200);
  g_settings.wheel_circumference_mm = prefs.getUShort("wheel", 2096);
  g_settings.sd_logging_enabled = prefs.getBool("sdlog", true);
  
  String csc = prefs.getString("csc_mac", "");
  String hr = prefs.getString("hr_mac", "");
  String pow = prefs.getString("pow_mac", "");

  snprintf(g_settings.paired_csc_mac, sizeof(g_settings.paired_csc_mac), "%s", csc.c_str());
  snprintf(g_settings.paired_hr_mac, sizeof(g_settings.paired_hr_mac), "%s", hr.c_str());
  snprintf(g_settings.paired_power_mac, sizeof(g_settings.paired_power_mac), "%s", pow.c_str());

  prefs.end();

  Serial.printf("[SETTINGS] Loaded NVS: Units=%u, Brightness=%u, Wheel=%u, CSC=%s, HR=%s, POW=%s\n",
                g_settings.units, g_settings.brightness, g_settings.wheel_circumference_mm,
                g_settings.paired_csc_mac, g_settings.paired_hr_mac, g_settings.paired_power_mac);
}

void saveSettings() {
  prefs.begin("opencyclo", false);
  prefs.putUChar("units", g_settings.units);
  prefs.putUChar("bright", g_settings.brightness);
  prefs.putUShort("wheel", g_settings.wheel_circumference_mm);
  prefs.putBool("sdlog", g_settings.sd_logging_enabled);
  prefs.putString("csc_mac", g_settings.paired_csc_mac);
  prefs.putString("hr_mac", g_settings.paired_hr_mac);
  prefs.putString("pow_mac", g_settings.paired_power_mac);
  prefs.end();

  Serial.println("[SETTINGS] Saved NVS preferences!");
}
