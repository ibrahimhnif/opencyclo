#include "settings_page.h"
#include "storage/settings.h"
#include <stdio.h>

static uint16_t COLOR_BG       = tft.color565(12, 16, 26);
static uint16_t COLOR_CARD     = tft.color565(26, 34, 52);
static uint16_t COLOR_CARD_ACC = tft.color565(36, 46, 68);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_TEXT_MUT = tft.color565(140, 155, 180);

void renderSettingsPage(const TelemetryState& state, bool forceFullRedraw) {
  if (forceFullRedraw) {
    tft.fillScreen(COLOR_BG);

    // Header Title
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.setTextSize(2);
    tft.setCursor(8, 6);
    tft.print("SETTINGS & SYSTEM");

    // Container Card
    tft.fillRoundRect(6, 28, 228, 246, 8, COLOR_CARD);
    tft.drawRoundRect(6, 28, 228, 246, 8, COLOR_CARD_ACC);
  }

  int y = 38;
  tft.setTextSize(1);

  // Row 1: Units
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("SPEED UNITS:");
  tft.fillRoundRect(120, y - 4, 100, 20, 4, COLOR_CYAN);
  tft.setTextColor(TFT_BLACK, COLOR_CYAN);
  tft.setCursor(132, y);
  tft.print(g_settings.units == 0 ? "METRIC (KM/H)" : "IMPERIAL (MPH)");
  y += 32;

  // Row 2: Brightness
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("BRIGHTNESS:");
  tft.fillRoundRect(120, y - 4, 100, 20, 4, COLOR_CARD_ACC);
  tft.setTextColor(TFT_WHITE, COLOR_CARD_ACC);
  tft.setCursor(150, y);
  char brightStr[16];
  snprintf(brightStr, sizeof(brightStr), "%u%%", (g_settings.brightness * 100) / 255);
  tft.print(brightStr);
  y += 32;

  // Row 3: Wheel Size
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("WHEEL SIZE:");
  tft.setCursor(120, y);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  char wheelStr[20];
  snprintf(wheelStr, sizeof(wheelStr), "%u mm (700x25c)", g_settings.wheel_circumference_mm);
  tft.print(wheelStr);
  y += 32;

  // Row 4: SD Logging
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("SD LOGGING:");
  tft.setCursor(120, y);
  tft.setTextColor(g_settings.sd_logging_enabled ? COLOR_GREEN : COLOR_TEXT_MUT, COLOR_CARD);
  tft.print(g_settings.sd_logging_enabled ? "ENABLED (GPX)" : "DISABLED");
  y += 32;

  // Row 5: Battery Level
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("BATTERY LEVEL:");
  tft.setCursor(120, y);
  tft.setTextColor(COLOR_GREEN, COLOR_CARD);
  char batStr[16];
  snprintf(batStr, sizeof(batStr), "%u%% (4.12 V)", state.battery_pct);
  tft.print(batStr);
  y += 32;

  // Row 6: Device Info
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("FIRMWARE:");
  tft.setCursor(120, y);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD);
  tft.print("v0.1.0 (ESP32-S3 N16R8)");
}

bool handleSettingsPageTouch(int16_t x, int16_t y) {
  // Toggle Units Button (x: 120..220, y: 34..54)
  if (x >= 120 && x <= 220 && y >= 34 && y <= 54) {
    g_settings.units = (g_settings.units == 0) ? 1 : 0;
    saveSettings();
    return true;
  }
  // Toggle Brightness Button (x: 120..220, y: 66..86)
  if (x >= 120 && x <= 220 && y >= 66 && y <= 86) {
    if (g_settings.brightness >= 255) {
      g_settings.brightness = 80;
    } else {
      g_settings.brightness += 60;
      if (g_settings.brightness > 255) g_settings.brightness = 255;
    }
    setDisplayBrightness(g_settings.brightness);
    saveSettings();
    return true;
  }
  return false;
}
