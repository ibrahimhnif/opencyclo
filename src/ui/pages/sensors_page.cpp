#include "sensors_page.h"
#include "hardware/ble_task.h"
#include "storage/settings.h"
#include <stdio.h>

static uint16_t COLOR_BG       = tft.color565(10, 14, 24);
static uint16_t COLOR_CARD     = tft.color565(20, 28, 44);
static uint16_t COLOR_CARD_ACC = tft.color565(32, 44, 68);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_RED      = tft.color565(255, 71, 87);
static uint16_t COLOR_TEXT_MUT = tft.color565(140, 155, 180);

void renderSensorsPage(const TelemetryState& state, bool forceFullRedraw) {
  if (forceFullRedraw) {
    tft.fillScreen(COLOR_BG);

    // 1. TOP HEADER (y: 0 .. 24)
    tft.fillRect(0, 0, 240, 24, COLOR_CARD);
    tft.drawFastHLine(0, 24, 240, COLOR_CARD_ACC);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, COLOR_CARD);
    tft.setCursor(8, 7);
    tft.print("BLE SENSORS & GPS");

    // Page Indicator Dots (Center: x: 104..136)
    for (int i = 0; i < 5; i++) {
      uint16_t dotColor = (i == 2) ? COLOR_CYAN : COLOR_CARD_ACC;
      tft.fillCircle(104 + (i * 8), 12, (i == 2) ? 3 : 2, dotColor);
    }

    // 2. MAIN CONTAINER CARD (x: 4, y: 28, w: 232, h: 288, FULL HEIGHT!)
    tft.fillRoundRect(4, 28, 232, 288, 8, COLOR_CARD);
    tft.drawRoundRect(4, 28, 232, 288, 8, COLOR_CARD_ACC);
  }

  // Row 1: Scan Action Button (y: 34..62)
  uint16_t scanBtnColor = g_ble_scanning ? COLOR_AMBER : COLOR_CYAN;
  tft.fillRoundRect(10, 34, 220, 28, 6, scanBtnColor);
  tft.setTextColor(TFT_BLACK, scanBtnColor);
  tft.setTextSize(1);
  tft.setCursor(56, 44);
  tft.print(g_ble_scanning ? "SCANNING BLE..." : "SCAN & ADD SENSORS");

  int y = 74;

  // Row 2: SPEED / CADENCE (CSC)
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("SPEED/CAD:");
  tft.setCursor(92, y);
  if (g_settings.paired_csc_mac[0] != '\0') {
    tft.setTextColor(COLOR_GREEN, COLOR_CARD);
    tft.printf("%.10s..", g_settings.paired_csc_mac);
    tft.fillRoundRect(172, y - 4, 52, 18, 4, COLOR_RED);
    tft.setTextColor(TFT_WHITE, COLOR_RED);
    tft.setCursor(178, y);
    tft.print("FORGET");
  } else {
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.print("NOT PAIRED");
  }
  y += 32;

  // Row 3: HEART RATE (HR)
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("HEART RATE:");
  tft.setCursor(92, y);
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
  y += 32;

  // Row 4: POWER METER
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("POWER METER:");
  tft.setCursor(92, y);
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
  y += 36;

  // Separator line
  tft.drawFastHLine(12, y, 216, COLOR_CARD_ACC);
  y += 12;

  // GPS Diagnostics Summary
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("GPS FIX STATUS:");
  tft.setCursor(110, y);
  tft.setTextColor(state.gps_has_fix ? COLOR_GREEN : COLOR_AMBER, COLOR_CARD);
  tft.print(state.gps_has_fix ? "3D FIX VALID" : "SEARCHING...");
  y += 24;

  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("SATELLITES:");
  tft.setCursor(110, y);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD);
  char satStr[32];
  snprintf(satStr, sizeof(satStr), "%u sats (HDOP: %.2f)", state.satellites, state.hdop);
  tft.print(satStr);
  y += 24;

  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, y);
  tft.print("COORDINATES:");
  tft.setCursor(110, y);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  char coordStr[32];
  snprintf(coordStr, sizeof(coordStr), "%.5f, %.5f", state.lat, state.lon);
  tft.print(coordStr);
}

bool handleSensorsPageTouch(int16_t x, int16_t y) {
  // Scan Button Tap (x: 10..230, y: 34..62)
  if (x >= 10 && x <= 230 && y >= 34 && y <= 62) {
    triggerBleScan();
    return true;
  }
  // CSC Forget Button Tap (x: 172..224, y: 70..90)
  if (x >= 172 && x <= 224 && y >= 70 && y <= 90) {
    forgetSensorProfile(BLE_PROFILE_CSC);
    return true;
  }
  // HR Forget Button Tap (x: 172..224, y: 100..120)
  if (x >= 172 && x <= 224 && y >= 100 && y <= 120) {
    forgetSensorProfile(BLE_PROFILE_HR);
    return true;
  }
  // Power Forget Button Tap (x: 172..224, y: 130..150)
  if (x >= 172 && x <= 224 && y >= 130 && y <= 150) {
    forgetSensorProfile(BLE_PROFILE_POWER);
    return true;
  }
  return false;
}
