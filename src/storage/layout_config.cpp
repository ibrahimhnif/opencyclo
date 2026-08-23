#include "layout_config.h"
#include "ui/engine/widget_catalog.h"
#include <Preferences.h>
#include <SD_MMC.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

UiConfig g_ui_config;
static Preferences uiPrefs;

void resetLayoutToDefaults() {
  g_ui_config.schema_version = UI_CONFIG_SCHEMA_VERSION;
  g_ui_config.active_page_count = 5;

  // Page 0: Ride (Hero 6-Grid)
  snprintf(g_ui_config.pages[0].title, sizeof(g_ui_config.pages[0].title), "ride");
  g_ui_config.pages[0].template_id = TEMPLATE_HERO_6_GRID;
  g_ui_config.pages[0].widget_count = 6;
  g_ui_config.pages[0].widgets[0] = WIDGET_SPEED;
  g_ui_config.pages[0].widgets[1] = WIDGET_DISTANCE;
  g_ui_config.pages[0].widgets[2] = WIDGET_RIDE_TIME;
  g_ui_config.pages[0].widgets[3] = WIDGET_CADENCE;
  g_ui_config.pages[0].widgets[4] = WIDGET_HEART_RATE;
  g_ui_config.pages[0].widgets[5] = WIDGET_POWER;

  // Page 1: Climb (2-Grid + Chart)
  snprintf(g_ui_config.pages[1].title, sizeof(g_ui_config.pages[1].title), "climb");
  g_ui_config.pages[1].template_id = TEMPLATE_2_GRID_CHART;
  g_ui_config.pages[1].widget_count = 3;
  g_ui_config.pages[1].widgets[0] = WIDGET_ALTITUDE;
  g_ui_config.pages[1].widgets[1] = WIDGET_GRADE;
  g_ui_config.pages[1].widgets[2] = WIDGET_ELEVATION_CHART;

  // Page 2: Sensors (Full Container)
  snprintf(g_ui_config.pages[2].title, sizeof(g_ui_config.pages[2].title), "sensors");
  g_ui_config.pages[2].template_id = TEMPLATE_FULL_CONTAINER;
  g_ui_config.pages[2].widget_count = 1;
  g_ui_config.pages[2].widgets[0] = WIDGET_BLE_MANAGER;

  // Page 3: Settings (Full Container)
  snprintf(g_ui_config.pages[3].title, sizeof(g_ui_config.pages[3].title), "settings");
  g_ui_config.pages[3].template_id = TEMPLATE_FULL_CONTAINER;
  g_ui_config.pages[3].widget_count = 1;
  g_ui_config.pages[3].widgets[0] = WIDGET_SETTINGS_LIST;

  // Page 4: Camera (Full Container) -- Insta360 Ace Pro 2 remote trigger.
  // Best-effort: verified against the older X3/RS protocol only, not this
  // camera specifically. See ble_camera_remote.cpp.
  snprintf(g_ui_config.pages[4].title, sizeof(g_ui_config.pages[4].title), "camera");
  g_ui_config.pages[4].template_id = TEMPLATE_FULL_CONTAINER;
  g_ui_config.pages[4].widget_count = 1;
  g_ui_config.pages[4].widgets[0] = WIDGET_CAMERA_REMOTE;

  Serial.printf("[LAYOUT CONFIG] Loaded Factory Default Page Tree (5 pages, schema v%u).\n", UI_CONFIG_SCHEMA_VERSION);
}

void initLayoutConfig() {
  initWidgetRegistry();

  uiPrefs.begin("opencyclo_ui", false);
  size_t readBytes = uiPrefs.getBytes("config", &g_ui_config, sizeof(UiConfig));
  uiPrefs.end();

  if (!isUiConfigValid(g_ui_config, readBytes)) {
    Serial.println("[LAYOUT CONFIG] NVS layout missing, invalid, or from an older schema — resetting to defaults.");
    resetLayoutToDefaults();
    saveLayoutConfig();
  } else {
    Serial.printf("[LAYOUT CONFIG] Loaded %u custom pages from NVS Flash (schema v%u).\n",
                  g_ui_config.active_page_count, g_ui_config.schema_version);
  }
}

void saveLayoutConfig() {
  uiPrefs.begin("opencyclo_ui", false);
  uiPrefs.putBytes("config", &g_ui_config, sizeof(UiConfig));
  uiPrefs.end();
  Serial.println("[LAYOUT CONFIG] Saved UiConfig to NVS Flash.");
}

size_t exportLayoutToString(char* buffer, size_t maxLen) {
  // "schema" lets a companion app tell which firmware widget/template numbering
  // this payload uses before it tries to interpret the ids below.
  int written = snprintf(buffer, maxLen, "{\"page_count\":%u,\"schema\":%u,\"pages\":[",
                         g_ui_config.active_page_count, (unsigned)UI_CONFIG_SCHEMA_VERSION);
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

// Imports a layout sent over BLE / read off SD. The payload is untrusted: it can
// come from an older or newer companion-app build whose widget and template
// numbering does not match this firmware's enums. Everything is therefore parsed
// into a local staging copy and fully validated first — g_ui_config is only
// touched (and only persisted) once the entire payload checks out, so a rejected
// import can never leave the live layout half-updated.
bool importLayoutFromString(const char* jsonStr) {
  if (jsonStr == nullptr || strlen(jsonStr) < 10) return false;

  // Simple token parser for layout configuration
  const char* pCount = strstr(jsonStr, "\"page_count\":");
  if (!pCount) {
    Serial.println("[LAYOUT CONFIG] Import rejected: missing \"page_count\".");
    return false;
  }

  uint8_t pageCount = (uint8_t)atoi(pCount + 13);
  if (pageCount == 0 || pageCount > MAX_PAGES) {
    Serial.printf("[LAYOUT CONFIG] Import rejected: page_count %u outside 1..%d.\n",
                  pageCount, (int)MAX_PAGES);
    return false;
  }

  const char* pPages = strstr(jsonStr, "\"pages\":[");
  if (!pPages) {
    Serial.println("[LAYOUT CONFIG] Import rejected: missing \"pages\" array.");
    return false;
  }

  UiConfig staged;
  memset(&staged, 0, sizeof(staged));
  staged.active_page_count = pageCount;

  const char* cursor = pPages + 9;
  for (uint8_t i = 0; i < pageCount; i++) {
    const char* pObj = strchr(cursor, '{');
    if (!pObj) {
      Serial.printf("[LAYOUT CONFIG] Import rejected: found %u of %u page objects.\n", i, pageCount);
      return false;
    }
    PageConfig& page = staged.pages[i];

    // Parse title
    const char* pTitle = strstr(pObj, "\"title\":\"");
    if (pTitle) {
      const char* tStart = pTitle + 9;
      const char* tEnd = strchr(tStart, '\"');
      if (tEnd) {
        size_t len = tEnd - tStart;
        if (len >= sizeof(page.title)) len = sizeof(page.title) - 1;
        strncpy(page.title, tStart, len);
        page.title[len] = '\0';
      }
    }

    // Parse template. Required and range-checked: the template selects the slot
    // geometry every widget on this page is validated against below.
    const char* pTemplate = strstr(pObj, "\"template\":");
    if (!pTemplate) {
      Serial.printf("[LAYOUT CONFIG] Import rejected: page %u has no \"template\".\n", i);
      return false;
    }
    int templateId = atoi(pTemplate + 11);
    if (templateId < 0 || templateId >= (int)TEMPLATE_COUNT) {
      Serial.printf("[LAYOUT CONFIG] Import rejected: page %u template %d outside 0..%d.\n",
                    i, templateId, (int)TEMPLATE_COUNT - 1);
      return false;
    }
    page.template_id = (LayoutTemplateId)templateId;
    const TemplateSlotDefinition& slotDef = getTemplateDefinition(page.template_id);

    // Parse widgets
    const char* pWidgets = strstr(pObj, "\"widgets\":[");
    if (pWidgets) {
      const char* wCursor = pWidgets + 11;
      uint8_t wIdx = 0;
      while (wCursor && *wCursor != ']' && wIdx < MAX_SLOTS_PER_PAGE) {
        int widgetId = atoi(wCursor);
        if (widgetId < 0 || widgetId >= (int)WIDGET_TYPE_COUNT) {
          Serial.printf("[LAYOUT CONFIG] Import rejected: page %u slot %u widget id %d outside 0..%d.\n",
                        i, wIdx, widgetId, (int)WIDGET_TYPE_COUNT - 1);
          return false;
        }
        page.widgets[wIdx++] = (WidgetType)widgetId;
        const char* nextComma = strchr(wCursor, ',');
        const char* endBracket = strchr(wCursor, ']');
        if (nextComma && (!endBracket || nextComma < endBracket)) {
          wCursor = nextComma + 1;
        } else {
          break;
        }
      }
      // Extra widgets are clamped away rather than rejected — the parse loop
      // above already bounds by MAX_SLOTS_PER_PAGE, this bounds by what the
      // chosen template can actually display.
      if (wIdx > slotDef.max_slots) {
        Serial.printf("[LAYOUT CONFIG] Page %u: %u widgets clamped to template's %u slots.\n",
                      i, wIdx, slotDef.max_slots);
        wIdx = slotDef.max_slots;
      }
      page.widget_count = wIdx;
    }

    // Same contract renderWidget()/handleWidgetTouch() enforce at runtime: a
    // widget may only sit in a slot whose size class it declares support for.
    for (uint8_t j = 0; j < page.widget_count; j++) {
      WidgetType w = page.widgets[j];
      if (w == WIDGET_NONE) continue; // an empty slot is legal — nothing is drawn
      if (!widgetSupportsSize(w, slotDef.slots[j].size_class)) {
        Serial.printf("[LAYOUT CONFIG] Import rejected: page %u slot %u widget %u unsupported in size class %u.\n",
                      i, j, (unsigned)w, (unsigned)slotDef.slots[j].size_class);
        return false;
      }
    }

    const char* objEnd = strchr(pObj, '}');
    if (!objEnd) {
      if (i + 1 < pageCount) {
        Serial.printf("[LAYOUT CONFIG] Import rejected: page %u object is unterminated.\n", i);
        return false;
      }
      break;
    }
    cursor = objEnd + 1;
  }

  // Fully validated — publish atomically, stamping this firmware's schema.
  g_ui_config = staged;
  g_ui_config.schema_version = UI_CONFIG_SCHEMA_VERSION;
  saveLayoutConfig();
  Serial.printf("[LAYOUT CONFIG] Imported %u pages from BLE JSON string (schema v%u).\n",
                g_ui_config.active_page_count, (unsigned)UI_CONFIG_SCHEMA_VERSION);
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
