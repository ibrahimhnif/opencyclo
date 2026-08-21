#include "sensors_page.h"
#include "hardware/ble_task.h"
#include "storage/settings.h"
#include <stdio.h>

static uint16_t COLOR_BG       = tft.color565(12, 16, 26);
static uint16_t COLOR_CARD     = tft.color565(26, 34, 52);
static uint16_t COLOR_CARD_ACC = tft.color565(36, 46, 68);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_RED      = tft.color565(255, 71, 87);
static uint16_t COLOR_TEXT_MUT = tft.color565(140, 155, 180);

void renderSensorsPage(const TelemetryState& state, bool forceFullRedraw) {
  if (forceFullRedraw) {
    tft.fillScreen(COLOR_BG);

    // Header Title
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.setTextSize(2);
    tft.setCursor(8, 6);
    tft.print("BLE & GPS SENSORS");

    // Container Card for details
    tft.fillRoundRect(6, 28, 228, 246, 8, COLOR_CARD);
    tft.drawRoundRect(6, 28, 228, 246, 8, COLOR_CARD_ACC);
  }

  // Row 1: Scan Action Button (y: 34..58)
  uint16_t scanBtnColor = g_ble_scanning ? COLOR_AMBER : COLOR_CYAN;
  tft.fillRoundRect(12, 34, 216, 24, 6, scanBtnColor);
  tft.setTextColor(TFT_BLACK, scanBtnColor);
  tft.setTextSize(1);
  tft.setCursor(54, 42);
  tft.print(g_ble_scanning ? "SCANNING BLE..." : "SCAN & ADD SENSORS");

  int y = 68;

  // Row 2: SPEED / CADENCE (CSC)
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("SPEED/CAD:");
  tft.setCursor(95, y);
  if (g_settings.paired_csc_mac[0] != '\0') {
    tft.setTextColor(COLOR_GREEN, COLOR_CARD);
    tft.printf("%.10s..", g_settings.paired_csc_mac);
    // Forget button
    tft.fillRoundRect(172, y - 4, 52, 18, 4, COLOR_RED);
    tft.setTextColor(TFT_WHITE, COLOR_RED);
    tft.setCursor(178, y);
    tft.print("FORGET");
  } else {
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.print("NOT PAIRED");
  }
  y += 28;

  // Row 3: HEART RATE (HR)
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("HEART RATE:");
  tft.setCursor(95, y);
  if (g_settings.paired_hr_mac[0] != '\0') {
    tft.setTextColor(COLOR_GREEN, COLOR_CARD);
    tft.printf("%.10s..", g_settings.paired_hr_mac);
    tft.fillRoundRect(172, y - 4, 52, 18, 4, COLOR_RED);
    tft.setTextColor(TFT_WHITE, COLOR_RED);
    tft.setCursor(178, y);
    tft.print("FORGET");
  } else {
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.print("NOT PAIRED");
  }
  y += 28;

  // Row 4: POWER METER
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("POWER METER:");
  tft.setCursor(95, y);
  if (g_settings.paired_power_mac[0] != '\0') {
    tft.setTextColor(COLOR_GREEN, COLOR_CARD);
    tft.printf("%.10s..", g_settings.paired_power_mac);
    tft.fillRoundRect(172, y - 4, 52, 18, 4, COLOR_RED);
    tft.setTextColor(TFT_WHITE, COLOR_RED);
    tft.setCursor(178, y);
    tft.print("FORGET");
  } else {
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.print("NOT PAIRED");
  }
  y += 32;

  // Separator line
  tft.drawFastHLine(14, y, 212, COLOR_CARD_ACC);
  y += 8;

  // GPS Diagnostics Summary
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("GPS STATUS:");
  tft.setCursor(100, y);
  tft.setTextColor(state.gps_has_fix ? COLOR_GREEN : COLOR_AMBER, COLOR_CARD);
  tft.print(state.gps_has_fix ? "3D FIX VALID" : "SEARCHING...");
  y += 20;

  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("SATELLITES:");
  tft.setCursor(100, y);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD);
  char satStr[32];
  snprintf(satStr, sizeof(satStr), "%u sats (HDOP: %.2f)", state.satellites, state.hdop);
  tft.print(satStr);
  y += 20;

  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("COORDINATES:");
  tft.setCursor(100, y);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  char coordStr[32];
  snprintf(coordStr, sizeof(coordStr), "%.5f, %.5f", state.lat, state.lon);
  tft.print(coordStr);
}

bool handleSensorsPageTouch(int16_t x, int16_t y) {
  // Scan Button Tap (x: 12..228, y: 34..58)
  if (x >= 12 && x <= 228 && y >= 34 && y <= 58) {
    triggerBleScan();
    return true;
  }
  // CSC Forget Button Tap (x: 172..224, y: 64..82)
  if (x >= 172 && x <= 224 && y >= 64 && y <= 82) {
    forgetSensorProfile(BLE_PROFILE_CSC);
    return true;
  }
  // HR Forget Button Tap (x: 172..224, y: 92..110)
  if (x >= 172 && x <= 224 && y >= 92 && y <= 110) {
    forgetSensorProfile(BLE_PROFILE_HR);
    return true;
  }
  // Power Forget Button Tap (x: 172..224, y: 120..138)
  if (x >= 172 && x <= 224 && y >= 120 && y <= 138) {
    forgetSensorProfile(BLE_PROFILE_POWER);
    return true;
  }
  return false;
}
