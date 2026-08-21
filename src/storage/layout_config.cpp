#include "layout_config.h"
#include <Preferences.h>
#include <SD_MMC.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

size_t exportLayoutToString(char* buffer, size_t maxLen) {
  int written = snprintf(buffer, maxLen, "{\"page_count\":%u,\"pages\":[", g_ui_config.active_page_count);
  for (uint8_t i = 0; i < g_ui_config.active_page_count; i++) {
    const PageConfig& p = g_ui_config.pages[i];
    written += snprintf(buffer + written, maxLen - written,
      "%s{\"title\":\"%s\",\"template\":%u,\"widgets\":[",
      (i > 0) ? "," : "", p.title, p.template_id);
    for (uint8_t j = 0; j < p.widget_count; j++) {
      written += snprintf(buffer + written, maxLen - written, "%u%s",
        p.widgets[j], (j < p.widget_count - 1) ? "," : "");
    }
    written += snprintf(buffer + written, maxLen - written, "]}");
  }
  written += snprintf(buffer + written, maxLen - written, "]}");
  return (written > 0 && (size_t)written < maxLen) ? (size_t)written : 0;
}

bool importLayoutFromString(const char* jsonStr) {
  if (jsonStr == nullptr || strlen(jsonStr) < 10) return false;

  // Simple token parser for layout configuration
  const char* pCount = strstr(jsonStr, "\"page_count\":");
  if (!pCount) return false;

  uint8_t pageCount = (uint8_t)atoi(pCount + 13);
  if (pageCount == 0 || pageCount > MAX_PAGES) return false;

  g_ui_config.active_page_count = pageCount;

  const char* pPages = strstr(jsonStr, "\"pages\":[");
  if (!pPages) return false;

  const char* cursor = pPages + 9;
  for (uint8_t i = 0; i < pageCount; i++) {
    const char* pObj = strchr(cursor, '{');
    if (!pObj) break;

    // Parse title
    const char* pTitle = strstr(pObj, "\"title\":\"");
    if (pTitle) {
      const char* tStart = pTitle + 9;
      const char* tEnd = strchr(tStart, '\"');
      if (tEnd) {
        size_t len = tEnd - tStart;
        if (len >= sizeof(g_ui_config.pages[i].title)) len = sizeof(g_ui_config.pages[i].title) - 1;
        strncpy(g_ui_config.pages[i].title, tStart, len);
        g_ui_config.pages[i].title[len] = '\0';
      }
    }

    // Parse template
    const char* pTemplate = strstr(pObj, "\"template\":");
    if (pTemplate) {
      g_ui_config.pages[i].template_id = (LayoutTemplateId)atoi(pTemplate + 11);
    }

    // Parse widgets
    const char* pWidgets = strstr(pObj, "\"widgets\":[");
    if (pWidgets) {
      const char* wCursor = pWidgets + 11;
      uint8_t wIdx = 0;
      while (wCursor && *wCursor != ']' && wIdx < MAX_SLOTS_PER_PAGE) {
        g_ui_config.pages[i].widgets[wIdx++] = (WidgetType)atoi(wCursor);
        const char* nextComma = strchr(wCursor, ',');
        const char* endBracket = strchr(wCursor, ']');
        if (nextComma && (!endBracket || nextComma < endBracket)) {
          wCursor = nextComma + 1;
        } else {
          break;
        }
      }
      g_ui_config.pages[i].widget_count = wIdx;
    }

    const char* objEnd = strchr(pObj, '}');
    if (objEnd) cursor = objEnd + 1;
    else break;
  }

  saveLayoutConfig();
  Serial.printf("[LAYOUT CONFIG] Imported %u pages from BLE JSON string.\n", g_ui_config.active_page_count);
  return true;
}

bool exportLayoutToJson(const char* filepath) {
  File file = SD_MMC.open(filepath, FILE_WRITE);
  if (!file) return false;

  char buf[1024];
  size_t len = exportLayoutToString(buf, sizeof(buf));
  if (len > 0) {
    file.write((const uint8_t*)buf, len);
  }
  file.close();
  Serial.printf("[LAYOUT CONFIG] Exported layout to %s\n", filepath);
  return true;
}

bool importLayoutFromJson(const char* filepath) {
  File file = SD_MMC.open(filepath, FILE_READ);
  if (!file) return false;

  char buf[1024];
  size_t len = file.readBytes(buf, sizeof(buf) - 1);
  buf[len] = '\0';
  file.close();

  return importLayoutFromString(buf);
}
