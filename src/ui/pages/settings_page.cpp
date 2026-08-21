#include "settings_page.h"
#include "storage/settings.h"
#include "hardware/battery.h"
#include <stdio.h>

static uint16_t COLOR_BG       = tft.color565(10, 14, 24);
static uint16_t COLOR_CARD     = tft.color565(20, 28, 44);
static uint16_t COLOR_CARD_ACC = tft.color565(32, 44, 68);
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
    tft.print("SYSTEM PREFERENCES");

    // Page Indicator Dots (Center: x: 104..136)
    for (int i = 0; i < 5; i++) {
      uint16_t dotColor = (i == 4) ? COLOR_CYAN : COLOR_CARD_ACC;
      tft.fillCircle(104 + (i * 8), 12, (i == 4) ? 3 : 2, dotColor);
    }

    // 2. MAIN SETTINGS CONTAINER CARD (x: 4, y: 28, w: 232, h: 288, FULL HEIGHT!)
    tft.fillRoundRect(4, 28, 232, 288, 8, COLOR_CARD);
    tft.drawRoundRect(4, 28, 232, 288, 8, COLOR_CARD_ACC);
  }

  int y = 42;
  tft.setTextSize(1);

  // Row 1: Units Toggle
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("SPEED UNITS:");
  tft.fillRoundRect(110, y - 4, 116, 24, 4, COLOR_CYAN);
  tft.setTextColor(TFT_BLACK, COLOR_CYAN);
  tft.setCursor(120, y + 3);
  tft.print(g_settings.units == 0 ? "METRIC (KM/H)" : "IMPERIAL (MPH)");
  y += 38;

  // Row 2: Screen Brightness Control
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("BRIGHTNESS:");
  tft.fillRoundRect(110, y - 4, 116, 24, 4, COLOR_CARD_ACC);
  tft.setTextColor(TFT_WHITE, COLOR_CARD_ACC);
  tft.setCursor(145, y + 3);
  char brightStr[16];
  snprintf(brightStr, sizeof(brightStr), "%u%%", (g_settings.brightness * 100) / 255);
  tft.print(brightStr);
  y += 38;

  // Row 3: Wheel Size (700x25c)
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("WHEEL SIZE:");
  tft.setCursor(110, y);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  char wheelStr[24];
  snprintf(wheelStr, sizeof(wheelStr), "%u mm (700x25c)", g_settings.wheel_circumference_mm);
  tft.print(wheelStr);
  y += 38;

  // Row 4: SD Logging (GPX)
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("SD LOGGING:");
  tft.setCursor(110, y);
  tft.setTextColor(g_settings.sd_logging_enabled ? COLOR_GREEN : COLOR_AMBER, COLOR_CARD);
  tft.print(g_settings.sd_logging_enabled ? "ENABLED (GPX 1.1)" : "DISABLED");
  y += 38;

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
  y += 38;

  // Separator line
  tft.drawFastHLine(14, y - 8, 212, COLOR_CARD_ACC);

  // Firmware Info
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("FIRMWARE:");
  tft.setCursor(110, y);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD);
  tft.print("OpenCyclo v0.1.0");
}

bool handleSettingsPageTouch(int16_t x, int16_t y) {
  // Row 1: Speed Units (y: 38..62)
  if (y >= 38 && y <= 62) {
    g_settings.units = (g_settings.units == 0) ? 1 : 0;
    saveSettings();
    return true;
  }
  // Row 2: Brightness Cycle (y: 76..100)
  if (y >= 76 && y <= 100) {
    if (g_settings.brightness >= 250) g_settings.brightness = 50;
    else g_settings.brightness += 50;
    setDisplayBrightness(g_settings.brightness);
    saveSettings();
    return true;
  }
  // Row 4: SD Logging Toggle (y: 152..176)
  if (y >= 152 && y <= 176) {
    g_settings.sd_logging_enabled = !g_settings.sd_logging_enabled;
    saveSettings();
    return true;
  }
  return false;
}
