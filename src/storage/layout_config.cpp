#include "layout_config.h"
#include <Preferences.h>
#include <SD_MMC.h>
#include <stdio.h>

UiConfig g_ui_config;
static Preferences uiPrefs;

void resetLayoutToDefaults() {
  g_ui_config.active_page_count = 5;

  // Page 0: Ride (Hero 6-Grid)
  snprintf(g_ui_config.pages[0].title, sizeof(g_ui_config.pages[0].title), "RIDE TELEMETRY");
  g_ui_config.pages[0].template_id = TEMPLATE_HERO_6_GRID;
  g_ui_config.pages[0].widget_count = 6;
  g_ui_config.pages[0].widgets[0] = WIDGET_SPEED;
  g_ui_config.pages[0].widgets[1] = WIDGET_DISTANCE;
  g_ui_config.pages[0].widgets[2] = WIDGET_RIDE_TIME;
  g_ui_config.pages[0].widgets[3] = WIDGET_CADENCE;
  g_ui_config.pages[0].widgets[4] = WIDGET_HEART_RATE;
  g_ui_config.pages[0].widgets[5] = WIDGET_POWER;

  // Page 1: Climb (2-Grid + Chart)
  snprintf(g_ui_config.pages[1].title, sizeof(g_ui_config.pages[1].title), "CLIMB & ELEVATION");
  g_ui_config.pages[1].template_id = TEMPLATE_2_GRID_CHART;
  g_ui_config.pages[1].widget_count = 3;
  g_ui_config.pages[1].widgets[0] = WIDGET_ALTITUDE;
  g_ui_config.pages[1].widgets[1] = WIDGET_GRADE;
  g_ui_config.pages[1].widgets[2] = WIDGET_ELEVATION_CHART;

  // Page 2: Sensors (Full Container)
  snprintf(g_ui_config.pages[2].title, sizeof(g_ui_config.pages[2].title), "BLE & GPS SENSORS");
  g_ui_config.pages[2].template_id = TEMPLATE_FULL_CONTAINER;
  g_ui_config.pages[2].widget_count = 1;
  g_ui_config.pages[2].widgets[0] = WIDGET_BLE_MANAGER;

  // Page 3: NMEA Console (Full Container)
  snprintf(g_ui_config.pages[3].title, sizeof(g_ui_config.pages[3].title), "NMEA LIVE CONSOLE");
  g_ui_config.pages[3].template_id = TEMPLATE_FULL_CONTAINER;
  g_ui_config.pages[3].widget_count = 1;
  g_ui_config.pages[3].widgets[0] = WIDGET_NMEA_CONSOLE;

  // Page 4: Settings (Full Container)
  snprintf(g_ui_config.pages[4].title, sizeof(g_ui_config.pages[4].title), "SYSTEM PREFERENCES");
  g_ui_config.pages[4].template_id = TEMPLATE_FULL_CONTAINER;
  g_ui_config.pages[4].widget_count = 1;
  g_ui_config.pages[4].widgets[0] = WIDGET_SETTINGS_LIST;

  Serial.println("[LAYOUT CONFIG] Loaded Factory Default Page Tree (5 pages).");
}

void initLayoutConfig() {
  initWidgetRegistry();

  uiPrefs.begin("opencyclo_ui", false);
  size_t readBytes = uiPrefs.getBytes("config", &g_ui_config, sizeof(UiConfig));
  uiPrefs.end();

  if (readBytes != sizeof(UiConfig) || g_ui_config.active_page_count == 0 || g_ui_config.active_page_count > MAX_PAGES) {
    Serial.println("[LAYOUT CONFIG] NVS layout not found or invalid, initializing defaults...");
    resetLayoutToDefaults();
    saveLayoutConfig();
  } else {
    Serial.printf("[LAYOUT CONFIG] Loaded %u custom pages from NVS Flash.\n", g_ui_config.active_page_count);
  }
}

void saveLayoutConfig() {
  uiPrefs.begin("opencyclo_ui", false);
  uiPrefs.putBytes("config", &g_ui_config, sizeof(UiConfig));
  uiPrefs.end();
  Serial.println("[LAYOUT CONFIG] Saved UiConfig to NVS Flash.");
}

bool exportLayoutToJson(const char* filepath) {
  File file = SD_MMC.open(filepath, FILE_WRITE);
  if (!file) return false;

  file.printf("{\n  \"page_count\": %u,\n  \"pages\": [\n", g_ui_config.active_page_count);
  for (uint8_t i = 0; i < g_ui_config.active_page_count; i++) {
    const PageConfig& p = g_ui_config.pages[i];
    file.printf("    {\n      \"title\": \"%s\",\n      \"template\": %u,\n      \"widgets\": [",
                p.title, p.template_id);
    for (uint8_t j = 0; j < p.widget_count; j++) {
      file.printf("%u%s", p.widgets[j], (j < p.widget_count - 1) ? ", " : "");
    }
    file.printf("]\n    }%s\n", (i < g_ui_config.active_page_count - 1) ? "," : "");
  }
  file.print("  ]\n}\n");
  file.close();
  Serial.printf("[LAYOUT CONFIG] Exported layout to %s\n", filepath);
  return true;
}

bool importLayoutFromJson(const char* filepath) {
  // Reserved for companion app / SD JSON parser
  return false;
}
