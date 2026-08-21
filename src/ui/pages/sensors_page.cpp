#include "sensors_page.h"
#include <stdio.h>

static uint16_t COLOR_BG       = tft.color565(12, 16, 26);
static uint16_t COLOR_CARD     = tft.color565(26, 34, 52);
static uint16_t COLOR_CARD_ACC = tft.color565(36, 46, 68);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_TEXT_MUT = tft.color565(140, 155, 180);

void renderGpsInfoPage(const TelemetryState& state, bool forceFullRedraw) {
  if (forceFullRedraw) {
    tft.fillScreen(COLOR_BG);

    // Header Title
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.setTextSize(2);
    tft.setCursor(8, 6);
    tft.print("GPS DIAGNOSTICS");

    // Container Card for details
    tft.fillRoundRect(6, 28, 228, 246, 8, COLOR_CARD);
    tft.drawRoundRect(6, 28, 228, 246, 8, COLOR_CARD_ACC);
  }

  int y = 38;
  tft.setTextSize(1);

  // Row 1: Fix Status
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("FIX STATUS:");
  tft.setCursor(120, y);
  if (state.gps_has_fix) {
    tft.setTextColor(COLOR_GREEN, COLOR_CARD);
    tft.print("3D FIX (VALID)");
  } else {
    tft.setTextColor(COLOR_AMBER, COLOR_CARD);
    tft.print("SEARCHING...");
  }
  y += 24;

  // Row 2: Satellites
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("SATELLITES:");
  tft.setCursor(120, y);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD);
  char satStr[16];
  snprintf(satStr, sizeof(satStr), "%u sats", state.satellites);
  tft.print(satStr);
  y += 24;

  // Row 3: HDOP
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("HDOP:");
  tft.setCursor(120, y);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  char hdopStr[16];
  snprintf(hdopStr, sizeof(hdopStr), "%.2f", state.hdop);
  tft.print(hdopStr);
  y += 24;

  // Row 4: Latitude
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("LATITUDE:");
  tft.setCursor(120, y);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  char latStr[20];
  snprintf(latStr, sizeof(latStr), "%.6f", state.lat);
  tft.print(latStr);
  y += 24;

  // Row 5: Longitude
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("LONGITUDE:");
  tft.setCursor(120, y);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  char lonStr[20];
  snprintf(lonStr, sizeof(lonStr), "%.6f", state.lon);
  tft.print(lonStr);
  y += 24;

  // Row 6: Altitude
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("ALTITUDE:");
  tft.setCursor(120, y);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  char altStr[16];
  snprintf(altStr, sizeof(altStr), "%.1f m", state.altitude_m);
  tft.print(altStr);
  y += 24;

  // Row 7: Speed
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("SPEED:");
  tft.setCursor(120, y);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  char spdStr[16];
  snprintf(spdStr, sizeof(spdStr), "%.1f km/h", state.speed_kmh);
  tft.print(spdStr);
  y += 24;

  // Row 8: UART Config
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(16, y);
  tft.print("PORT CONFIG:");
  tft.setCursor(120, y);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.print("RX:44 TX:43 115200");
}
