#include "climb_page.h"
#include "storage/settings.h"
#include <stdio.h>

static uint16_t COLOR_BG       = tft.color565(10, 14, 24);
static uint16_t COLOR_CARD     = tft.color565(20, 28, 44);
static uint16_t COLOR_CARD_ACC = tft.color565(32, 44, 68);
static uint16_t COLOR_HERO_BG  = tft.color565(14, 22, 38);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_TEXT_MUT = tft.color565(140, 155, 180);

#define ELEV_SAMPLES 30
static float elevHistory[ELEV_SAMPLES];
static uint8_t elevHead = 0;
static bool elevFilled = false;

void renderClimbPage(const TelemetryState& state, bool forceFullRedraw) {
  // Push sample to history
  elevHistory[elevHead] = state.altitude_m;
  elevHead = (elevHead + 1) % ELEV_SAMPLES;
  if (elevHead == 0) elevFilled = true;

  if (forceFullRedraw) {
    tft.fillScreen(COLOR_BG);

    // 1. TOP HEADER (y: 0 .. 24)
    tft.fillRect(0, 0, 240, 24, COLOR_CARD);
    tft.drawFastHLine(0, 24, 240, COLOR_CARD_ACC);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, COLOR_CARD);
    tft.setCursor(8, 7);
    tft.print("CLIMB & ELEVATION PROFILE");

    // 2. HERO ALTITUDE CARD (x: 4, y: 28, w: 232, h: 74)
    tft.fillRoundRect(4, 28, 232, 74, 8, COLOR_HERO_BG);
    tft.drawRoundRect(4, 28, 232, 74, 8, COLOR_CYAN);

    // 3. MIDDLE GRID (x: 4 & 122, y: 106, w: 114, h: 58)
    tft.fillRoundRect(4, 106, 114, 58, 6, COLOR_CARD);
    tft.drawRoundRect(4, 106, 114, 58, 6, COLOR_CARD_ACC);

    tft.fillRoundRect(122, 106, 114, 58, 6, COLOR_CARD);
    tft.drawRoundRect(122, 106, 114, 58, 6, COLOR_CARD_ACC);

    // 4. SPARKLINE ELEVATION PROFILE CARD (x: 4, y: 168, w: 232, h: 108)
    tft.fillRoundRect(4, 168, 232, 108, 8, COLOR_CARD);
    tft.drawRoundRect(4, 168, 232, 108, 8, COLOR_CARD_ACC);
  }

  // --- HERO ALTITUDE CARD ---
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_HERO_BG);
  tft.setCursor(12, 34);
  tft.print("ALTITUDE");

  tft.setTextColor(TFT_WHITE, COLOR_HERO_BG);
  tft.setTextSize(3);
  char altBuf[12];
  float displayAlt = (g_settings.units == 1) ? (state.altitude_m * 3.28084f) : state.altitude_m;
  snprintf(altBuf, sizeof(altBuf), "%4.0f", displayAlt);
  tft.setCursor(12, 50);
  tft.print(altBuf);

  tft.setTextSize(2);
  tft.setTextColor(COLOR_CYAN, COLOR_HERO_BG);
  tft.setCursor(184, 58);
  tft.print((g_settings.units == 1) ? "FT" : "M");

  // --- MIDDLE GRID: GRADE % & TOTAL ASCENT ---
  // Left: Grade %
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(10, 112);
  tft.print("GRADE %");

  tft.setTextSize(2);
  uint16_t gradeColor = (state.grade_pct > 3.0f) ? COLOR_AMBER : ((state.grade_pct < -2.0f) ? COLOR_CYAN : COLOR_GREEN);
  tft.setTextColor(gradeColor, COLOR_CARD);
  tft.setCursor(10, 132);
  char gradeBuf[12];
  snprintf(gradeBuf, sizeof(gradeBuf), "%+4.1f%%", state.grade_pct);
  tft.print(gradeBuf);

  // Right: Total Ascent
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(128, 112);
  tft.print(g_settings.units == 1 ? "ASCENT (FT)" : "ASCENT (M)");

  tft.setTextSize(2);
  tft.setTextColor(COLOR_GREEN, COLOR_CARD);
  tft.setCursor(128, 132);
  char ascBuf[12];
  float displayAsc = (g_settings.units == 1) ? (state.total_ascent_m * 3.28084f) : state.total_ascent_m;
  snprintf(ascBuf, sizeof(ascBuf), "%.0f", displayAsc);
  tft.print(ascBuf);

  // --- SPARKLINE ELEVATION PROFILE CHART ---
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(12, 174);
  tft.print("LIVE ELEVATION PROFILE (30s)");

  // Find min and max altitude in buffer
  float minAlt = 99999.0f, maxAlt = -99999.0f;
  uint8_t count = elevFilled ? ELEV_SAMPLES : elevHead;
  if (count < 2) count = 2;

  for (uint8_t i = 0; i < count; i++) {
    float val = elevHistory[i];
    if (val < minAlt) minAlt = val;
    if (val > maxAlt) maxAlt = val;
  }
  if (maxAlt - minAlt < 5.0f) {
    maxAlt = minAlt + 5.0f;
  }

  // Chart bounds: x: 12..228, y: 190..264 (height: 74px)
  tft.fillRect(12, 188, 216, 80, COLOR_HERO_BG);
  tft.drawRect(12, 188, 216, 80, COLOR_CARD_ACC);

  int chartX = 14;
  int chartY = 190;
  int chartW = 212;
  int chartH = 76;

  int prevPx = -1, prevPy = -1;
  for (uint8_t i = 0; i < count; i++) {
    uint8_t idx = elevFilled ? ((elevHead + i) % ELEV_SAMPLES) : i;
    float val = elevHistory[idx];

    int px = chartX + (i * chartW) / (ELEV_SAMPLES - 1);
    int py = chartY + chartH - (int)(((val - minAlt) / (maxAlt - minAlt)) * (chartH - 4));

    if (prevPx != -1) {
      tft.drawLine(prevPx, prevPy, px, py, COLOR_GREEN);
      tft.drawLine(prevPx, prevPy + 1, px, py + 1, COLOR_GREEN); // Thicker line
    }
    prevPx = px;
    prevPy = py;
  }
}
