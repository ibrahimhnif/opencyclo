#include "debug_page.h"
#include <stdio.h>

static uint16_t COLOR_BG       = tft.color565(12, 16, 26);
static uint16_t COLOR_CARD     = tft.color565(26, 34, 52);
static uint16_t COLOR_CARD_ACC = tft.color565(36, 46, 68);
static uint16_t COLOR_CONSOLE  = tft.color565(5, 10, 15);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_TEXT_MUT = tft.color565(140, 155, 180);

void renderDebugPage(const TelemetryState& state, bool forceFullRedraw) {
  if (forceFullRedraw) {
    tft.fillScreen(COLOR_BG);

    // Header Title
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.setTextSize(2);
    tft.setCursor(8, 6);
    tft.print("NMEA LIVE CONSOLE");

    // Stats Bar Container (x: 6, y: 28, w: 228, h: 42)
    tft.fillRoundRect(6, 28, 228, 42, 6, COLOR_CARD);
    tft.drawRoundRect(6, 28, 228, 42, 6, COLOR_CARD_ACC);

    // Terminal Screen Container (x: 6, y: 74, w: 228, h: 200)
    tft.fillRoundRect(6, 74, 228, 200, 6, COLOR_CONSOLE);
    tft.drawRoundRect(6, 74, 228, 200, 6, COLOR_GREEN);
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

  // Live Terminal Screen Output (Lines y: 82 .. 260)
  tft.fillRect(10, 78, 220, 192, COLOR_CONSOLE);
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
        tft.setCursor(10, y);
        tft.print(g_gps_debug.last_lines[lineIdx]);
        y += 22;
      }
    }
  }
}
