#include "debug_page.h"
#include <stdio.h>

static uint16_t COLOR_BG       = tft.color565(10, 14, 24);
static uint16_t COLOR_CARD     = tft.color565(20, 28, 44);
static uint16_t COLOR_CARD_ACC = tft.color565(32, 44, 68);
static uint16_t COLOR_CONSOLE  = tft.color565(5, 10, 15);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_TEXT_MUT = tft.color565(140, 155, 180);

void renderDebugPage(const TelemetryState& state, bool forceFullRedraw) {
  if (forceFullRedraw) {
    tft.fillScreen(COLOR_BG);

    // 1. TOP HEADER (y: 0 .. 24)
    tft.fillRect(0, 0, 240, 24, COLOR_CARD);
    tft.drawFastHLine(0, 24, 240, COLOR_CARD_ACC);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, COLOR_CARD);
    tft.setCursor(8, 7);
    tft.print("NMEA LIVE CONSOLE");

    // 2. Stats Bar Container (x: 4, y: 28, w: 232, h: 42)
    tft.fillRoundRect(4, 28, 232, 42, 6, COLOR_CARD);
    tft.drawRoundRect(4, 28, 232, 42, 6, COLOR_CARD_ACC);

    // 3. EXPANDED Terminal Screen Container (x: 4, y: 74, w: 232, h: 228)
    tft.fillRoundRect(4, 74, 232, 228, 6, COLOR_CONSOLE);
    tft.drawRoundRect(4, 74, 232, 228, 6, COLOR_GREEN);

    // 4. BOTTOM PAGE INDICATOR DOTS (y: 310)
    for (int i = 0; i < 5; i++) {
      uint16_t dotColor = (i == 3) ? COLOR_CYAN : COLOR_CARD_ACC;
      tft.fillCircle(104 + (i * 8), 310, (i == 3) ? 3 : 2, dotColor);
    }
  }

  // Stats Bar Info
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, 34);
  tft.print("PORT:");
  tft.setTextColor(COLOR_CYAN, COLOR_CARD);
  tft.printf(" RX:%u 115200", g_gps_debug.active_rx_pin);

  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(130, 34);
  tft.print("STATUS:");
  tft.setTextColor(state.gps_has_fix ? COLOR_GREEN : COLOR_AMBER, COLOR_CARD);
  tft.print(state.gps_has_fix ? "3D FIX" : "SEARCHING");

  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(14, 52);
  char statsStr[36];
  snprintf(statsStr, sizeof(statsStr), "Chars: %lu | Sentences: %lu",
           (unsigned long)g_gps_debug.total_chars, (unsigned long)g_gps_debug.sentences_passed);
  tft.print(statsStr);

  // Live Terminal Screen Output
  tft.fillRect(8, 78, 224, 220, COLOR_CONSOLE);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREEN, COLOR_CONSOLE);

  if (g_gps_debug.total_chars == 0) {
    tft.setTextColor(COLOR_AMBER, COLOR_CONSOLE);
    tft.setCursor(14, 90);
    tft.print("WAITING FOR UART BYTES...");
    tft.setCursor(14, 110);
    tft.print("Check RX/TX JST Cable");
  } else {
    int y = 82;
    for (int i = 0; i < NMEA_BUFFER_LINES; i++) {
      int lineIdx = (g_gps_debug.line_head + i) % NMEA_BUFFER_LINES;
      if (strlen(g_gps_debug.last_lines[lineIdx]) > 0) {
        tft.setCursor(8, y);
        tft.print(g_gps_debug.last_lines[lineIdx]);
        y += 24;
      }
    }
  }
}
