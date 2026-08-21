#include "settings_page.h"
#include "storage/settings.h"
#include "hardware/battery.h"
#include <stdio.h>

static uint16_t COLOR_BG       = tft.color565(10, 14, 24);
static uint16_t COLOR_CARD     = tft.color565(20, 28, 44);
static uint16_t COLOR_CARD_ACC = tft.color565(32, 44, 68);
static uint16_t COLOR_HERO_BG  = tft.color565(14, 22, 38);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_TEXT_MUT = tft.color565(140, 155, 180);

void renderSettingsPage(const TelemetryState& state, bool forceFullRedraw) {
  if (forceFullRedraw) {
    tft.fillScreen(COLOR_BG);

    // 1. TOP HEADER (y: 0 .. 24)
    tft.fillRect(0, 0, 240, 24, COLOR_CARD);
    tft.drawFastHLine(0, 24, 240, COLOR_CARD_ACC);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, COLOR_CARD);
    tft.setCursor(8, 7);
    tft.print("SYSTEM & PREFERENCES");

    // 2. MAIN SETTINGS CONTAINER CARD (x: 4, y: 28, w: 232, h: 246)
    tft.fillRoundRect(4, 28, 232, 246, 8, COLOR_CARD);
    tft.drawRoundRect(4, 28, 232, 246, 8, COLOR_CARD_ACC);
  }

  int y = 38;
  tft.setTextSize(1);

  // Row 1: Units Toggle
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("SPEED UNITS:");
  tft.fillRoundRect(110, y - 4, 116, 22, 4, COLOR_CYAN);
  tft.setTextColor(TFT_BLACK, COLOR_CYAN);
  tft.setCursor(120, y + 2);
  tft.print(g_settings.units == 0 ? "METRIC (KM/H)" : "IMPERIAL (MPH)");
  y += 32;

  // Row 2: Screen Brightness Control
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("BRIGHTNESS:");
  tft.fillRoundRect(110, y - 4, 116, 22, 4, COLOR_CARD_ACC);
  tft.setTextColor(TFT_WHITE, COLOR_CARD_ACC);
  tft.setCursor(145, y + 2);
  char brightStr[16];
  snprintf(brightStr, sizeof(brightStr), "%u%%", (g_settings.brightness * 100) / 255);
  tft.print(brightStr);
  y += 32;

  // Row 3: Wheel Size (700x25c)
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("WHEEL SIZE:");
  tft.setCursor(110, y);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  char wheelStr[24];
  snprintf(wheelStr, sizeof(wheelStr), "%u mm (700x25c)", g_settings.wheel_circumference_mm);
  tft.print(wheelStr);
  y += 32;

  // Row 4: SD Logging (GPX)
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("SD LOGGING:");
  tft.setCursor(110, y);
  tft.setTextColor(g_settings.sd_logging_enabled ? COLOR_GREEN : COLOR_AMBER, COLOR_CARD);
  tft.print(g_settings.sd_logging_enabled ? "ENABLED (GPX 1.1)" : "DISABLED");
  y += 32;

  // Row 5: Battery Voltage & Percentage
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("BATTERY LEVEL:");
  tft.setCursor(110, y);
  tft.setTextColor(COLOR_GREEN, COLOR_CARD);
  char batStr[24];
  float vBat = readBatteryVoltage();
  snprintf(batStr, sizeof(batStr), "%u%% (%.2f V)", state.battery_pct, vBat);
  tft.print(batStr);
  y += 32;

  // Separator line
  tft.drawFastHLine(14, y - 8, 212, COLOR_CARD_ACC);

  // Firmware Info
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("FIRMWARE:");
  tft.setCursor(110, y);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD);
  tft.print("OpenCyclo v0.1.0-M7");
}

bool handleSettingsPageTouch(int16_t x, int16_t y) {
  // Row 1: Speed Units (y: 34..58)
  if (y >= 34 && y <= 58) {
    g_settings.units = (g_settings.units == 0) ? 1 : 0;
    saveSettings();
    return true;
  }
  // Row 2: Brightness Cycle (y: 66..90)
  if (y >= 66 && y <= 90) {
    if (g_settings.brightness >= 250) g_settings.brightness = 50;
    else g_settings.brightness += 50;
    setDisplayBrightness(g_settings.brightness);
    saveSettings();
    return true;
  }
  // Row 4: SD Logging Toggle (y: 130..154)
  if (y >= 130 && y <= 154) {
    g_settings.sd_logging_enabled = !g_settings.sd_logging_enabled;
    saveSettings();
    return true;
  }
  return false;
}
