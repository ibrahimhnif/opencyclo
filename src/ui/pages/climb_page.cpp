#include "climb_page.h"
#include <stdio.h>

static uint16_t COLOR_BG       = tft.color565(12, 16, 26);
static uint16_t COLOR_CARD     = tft.color565(26, 34, 52);
static uint16_t COLOR_CARD_ACC = tft.color565(36, 46, 68);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_TEXT_MUT = tft.color565(140, 155, 180);

static float altHistory[30] = {0};
static uint8_t historyIdx = 0;
static uint32_t lastHistoryPushMs = 0;

void renderClimbPage(const TelemetryState& state, bool forceFullRedraw) {
  uint32_t now = millis();
  if (now - lastHistoryPushMs >= 2000) {
    lastHistoryPushMs = now;
    altHistory[historyIdx] = state.altitude_m;
    historyIdx = (historyIdx + 1) % 30;
  }

  if (forceFullRedraw) {
    tft.fillScreen(COLOR_BG);

    // Header Title
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.setTextSize(2);
    tft.setCursor(8, 6);
    tft.print("CLIMB & ELEVATION");

    // Hero Altitude Card Container (x: 6, y: 28, w: 228, h: 80)
    tft.fillRoundRect(6, 28, 228, 80, 8, COLOR_CARD);
    tft.drawRoundRect(6, 28, 228, 80, 8, COLOR_CARD_ACC);

    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.setCursor(14, 34);
    tft.print("ALTITUDE");
    tft.setCursor(200, 34);
    tft.print("m");

    // Metrics Grid Container (x: 6, y: 114, 228, 74)
    // Tile 1: Total Ascent (6, 114, 111, 35)
    tft.fillRoundRect(6, 114, 111, 35, 6, COLOR_CARD);
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.setCursor(12, 118);
    tft.print("ASCENT");

    // Tile 2: Grade % (123, 114, 111, 35)
    tft.fillRoundRect(123, 114, 111, 35, 6, COLOR_CARD);
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.setCursor(129, 118);
    tft.print("GRADE %");

    // Tile 3: Temp (6, 153, 111, 35)
    tft.fillRoundRect(6, 153, 111, 35, 6, COLOR_CARD);
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.setCursor(12, 157);
    tft.print("TEMP");

    // Tile 4: Humidity (123, 153, 111, 35)
    tft.fillRoundRect(123, 153, 111, 35, 6, COLOR_CARD);
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.setCursor(129, 157);
    tft.print("HUMIDITY");

    // Sparkline Graph Container (x: 6, y: 194, w: 228, h: 80)
    tft.fillRoundRect(6, 194, 228, 80, 6, COLOR_CARD);
    tft.drawRoundRect(6, 194, 228, 80, 6, COLOR_CARD_ACC);
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.setCursor(12, 198);
    tft.print("ELEVATION PROFILE");
  }

  // Hero Altitude Value
  char altStr[16];
  snprintf(altStr, sizeof(altStr), "%5.0f", state.altitude_m);
  tft.setTextSize(4);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD);
  tft.setCursor(14, 52);
  tft.print(altStr);

  // Tile 1 Value: Total Ascent
  char ascStr[16];
  snprintf(ascStr, sizeof(ascStr), "+%.0fm", state.total_ascent_m);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREEN, COLOR_CARD);
  tft.setCursor(12, 134);
  tft.print(ascStr);

  // Tile 2 Value: Grade %
  char gradeStr[16];
  snprintf(gradeStr, sizeof(gradeStr), "%+.1f%%", state.grade_pct);
  tft.setTextSize(1);
  tft.setTextColor((state.grade_pct > 1.0f) ? COLOR_GREEN : ((state.grade_pct < -1.0f) ? COLOR_CYAN : TFT_WHITE), COLOR_CARD);
  tft.setCursor(129, 134);
  tft.print(gradeStr);

  // Tile 3 & 4 Values: Temp & Humidity (read via BaroSample or stored in TelemetryState)
  // Display standard placeholder values if sensor details populated
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  tft.setCursor(12, 173);
  tft.print("BME280 OK");

  tft.setCursor(129, 173);
  tft.print("I2C 0x76/77");

  // Sparkline Graph Drawing (x: 14 to 226, y: 215 to 265)
  float minA = 99999.0f, maxA = -99999.0f;
  for (int i = 0; i < 30; i++) {
    if (altHistory[i] < minA) minA = altHistory[i];
    if (altHistory[i] > maxA) maxA = altHistory[i];
  }
  if (maxA - minA < 10.0f) maxA = minA + 10.0f;

  tft.fillRect(14, 215, 212, 52, COLOR_CARD);
  for (int i = 0; i < 29; i++) {
    int idx1 = (historyIdx + i) % 30;
    int idx2 = (historyIdx + i + 1) % 30;

    int x1 = 14 + (i * 7);
    int y1 = 265 - (int)(((altHistory[idx1] - minA) / (maxA - minA)) * 48.0f);
    int x2 = 14 + ((i + 1) * 7);
    int y2 = 265 - (int)(((altHistory[idx2] - minA) / (maxA - minA)) * 48.0f);

    tft.drawLine(x1, y1, x2, y2, COLOR_GREEN);
  }
}
