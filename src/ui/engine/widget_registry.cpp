#include "widget_registry.h"
#include "hardware/ble_task.h"
#include "hardware/battery.h"
#include "storage/settings.h"
#include <stdio.h>
#include <string.h>

static uint16_t COLOR_BG       = tft.color565(10, 14, 24);
static uint16_t COLOR_CARD     = tft.color565(20, 28, 44);
static uint16_t COLOR_CARD_ACC = tft.color565(32, 44, 68);
static uint16_t COLOR_HERO_BG  = tft.color565(14, 22, 38);
static uint16_t COLOR_CONSOLE  = tft.color565(5, 10, 15);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_RED      = tft.color565(255, 71, 87);
static uint16_t COLOR_TEXT_MUT = tft.color565(140, 155, 180);

// --- SPARKLINE BUFFER ---
#define ELEV_SAMPLES 30
static float s_elevHistory[ELEV_SAMPLES];
static uint8_t s_elevHead = 0;
static bool s_elevFilled = false;

// 1. SPEED WIDGET
static void renderWidgetSpeed(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, COLOR_HERO_BG);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 8, COLOR_CYAN);
  }
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_HERO_BG);
  tft.setCursor(b.x + 8, b.y + 6);
  tft.print("SPEED");

  tft.setCursor(b.x + b.w - 58, b.y + 6);
  tft.setTextColor(state.speed_source == SPEED_SOURCE_BLE_CSC ? COLOR_CYAN : COLOR_TEXT_MUT, COLOR_HERO_BG);
  tft.print(state.speed_source == SPEED_SOURCE_BLE_CSC ? "[BLE]" : "[GPS]");

  tft.setTextColor(TFT_WHITE, COLOR_HERO_BG);
  tft.setTextSize(4);
  char buf[12];
  float speed = (g_settings.units == 1) ? (state.speed_kmh * 0.621371f) : state.speed_kmh;
  snprintf(buf, sizeof(buf), "%4.1f", speed);
  tft.setCursor(b.x + 8, b.y + 22);
  tft.setTextPadding(144); // guards against ghosting if speed ever crosses 100
  tft.print(buf);
  tft.setTextPadding(0);

  tft.setTextSize(2);
  tft.setTextColor(COLOR_CYAN, COLOR_HERO_BG);
  tft.setCursor(b.x + b.w - 54, b.y + b.h - 22);
  tft.print((g_settings.units == 1) ? "MPH" : "KM/H");
}

// 2. AVG SPEED WIDGET
static void renderWidgetAvgSpeed(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD_ACC);
  }
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 6);
  tft.print("AVG SPD");

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 24);
  tft.setTextPadding(b.w - 12);
  char buf[12];
  float avg = (g_settings.units == 1) ? (state.avg_speed_kmh * 0.621371f) : state.avg_speed_kmh;
  snprintf(buf, sizeof(buf), "%.1f", avg);
  tft.print(buf);
  tft.setTextPadding(0);
}

// 3. MAX SPEED WIDGET
static void renderWidgetMaxSpeed(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD_ACC);
  }
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 6);
  tft.print("MAX SPD");

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 24);
  tft.setTextPadding(b.w - 12);
  char buf[12];
  float maxS = (g_settings.units == 1) ? (state.max_speed_kmh * 0.621371f) : state.max_speed_kmh;
  snprintf(buf, sizeof(buf), "%.1f", maxS);
  tft.print(buf);
  tft.setTextPadding(0);
}

// 4. DISTANCE WIDGET
static void renderWidgetDistance(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD_ACC);
  }
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 6);
  tft.print(g_settings.units == 1 ? "DIST (MI)" : "DIST (KM)");

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 24);
  tft.setTextPadding(b.w - 12);
  char buf[12];
  float d = (g_settings.units == 1) ? (state.trip_distance_km * 0.621371f) : state.trip_distance_km;
  snprintf(buf, sizeof(buf), "%.2f", d);
  tft.print(buf);
  tft.setTextPadding(0);
}

// 5. RIDE TIME WIDGET
static void renderWidgetRideTime(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD_ACC);
  }
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 6);
  tft.print("RIDE TIME");

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 24);
  char buf[12];
  uint32_t hrs = state.ride_time_s / 3600;
  uint32_t mins = (state.ride_time_s % 3600) / 60;
  uint32_t secs = state.ride_time_s % 60;
  if (hrs > 0) {
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hrs, mins, secs);
  } else {
    snprintf(buf, sizeof(buf), "%02u:%02u", mins, secs);
  }
  tft.setTextPadding(b.w - 12); // covers the MM:SS -> HH:MM:SS width jump past 1hr
  tft.print(buf);
  tft.setTextPadding(0);
}

// 6. CADENCE WIDGET
static void renderWidgetCadence(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD_ACC);
  }
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 6);
  tft.print("CAD(RPM)");

  tft.setTextSize(2);
  tft.setCursor(b.x + 6, b.y + 24);
  tft.setTextPadding(b.w - 12);
  if (state.cadence_rpm >= 0) {
    tft.setTextColor(COLOR_CYAN, COLOR_CARD);
    tft.printf("%d", state.cadence_rpm);
  } else {
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.print("--");
  }
  tft.setTextPadding(0);
}

// 7. HEART RATE WIDGET
static void renderWidgetHeartRate(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD_ACC);
  }
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 6);
  tft.print("HR (BPM)");

  tft.setTextSize(2);
  tft.setCursor(b.x + 6, b.y + 24);
  tft.setTextPadding(b.w - 12);
  if (state.heart_rate_bpm >= 0) {
    tft.setTextColor(COLOR_RED, COLOR_CARD);
    tft.printf("%d", state.heart_rate_bpm);
  } else {
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.print("--");
  }
  tft.setTextPadding(0);
}

// 8. POWER WIDGET
static void renderWidgetPower(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD_ACC);
  }
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 6);
  tft.print("PWR (W)");

  tft.setTextSize(2);
  tft.setCursor(b.x + 6, b.y + 24);
  tft.setTextPadding(b.w - 12);
  if (state.power_watts >= 0) {
    tft.setTextColor(COLOR_GREEN, COLOR_CARD);
    tft.printf("%d", state.power_watts);
  } else {
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.print("--");
  }
  tft.setTextPadding(0);
}

// 9. ALTITUDE WIDGET
static void renderWidgetAltitude(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD_ACC);
  }
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 6);
  tft.print(g_settings.units == 1 ? "ALT (FT)" : "ALT (M)");

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 24);
  tft.setTextPadding(b.w - 12);
  char buf[12];
  float alt = (g_settings.units == 1) ? (state.altitude_m * 3.28084f) : state.altitude_m;
  snprintf(buf, sizeof(buf), "%.0f", alt);
  tft.print(buf);
  tft.setTextPadding(0);
}

// 10. GRADE % WIDGET
static void renderWidgetGrade(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD_ACC);
  }
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 6);
  tft.print("GRADE %");

  tft.setTextSize(2);
  uint16_t gradeColor = (state.grade_pct > 3.0f) ? COLOR_AMBER : ((state.grade_pct < -2.0f) ? COLOR_CYAN : COLOR_GREEN);
  tft.setTextColor(gradeColor, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 24);
  tft.setTextPadding(b.w - 12);
  char buf[12];
  snprintf(buf, sizeof(buf), "%+4.1f%%", state.grade_pct);
  tft.print(buf);
  tft.setTextPadding(0);
}

// 11. TOTAL ASCENT WIDGET
static void renderWidgetTotalAscent(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD_ACC);
  }
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 6);
  tft.print(g_settings.units == 1 ? "ASCENT(FT)" : "ASCENT(M)");

  tft.setTextSize(2);
  tft.setTextColor(COLOR_GREEN, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 24);
  tft.setTextPadding(b.w - 12);
  char buf[12];
  float asc = (g_settings.units == 1) ? (state.total_ascent_m * 3.28084f) : state.total_ascent_m;
  snprintf(buf, sizeof(buf), "%.0f", asc);
  tft.print(buf);
  tft.setTextPadding(0);
}

// 12. ELEVATION SPARKLINE CHART WIDGET
static void renderWidgetElevationChart(const Rect& b, const TelemetryState& state, bool force) {
  s_elevHistory[s_elevHead] = state.altitude_m;
  s_elevHead = (s_elevHead + 1) % ELEV_SAMPLES;
  if (s_elevHead == 0) s_elevFilled = true;

  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 8, COLOR_CARD_ACC);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.setCursor(b.x + 8, b.y + 6);
    tft.print("LIVE ELEVATION PROFILE (30s)");
  }

  float minAlt = 99999.0f, maxAlt = -99999.0f;
  uint8_t count = s_elevFilled ? ELEV_SAMPLES : s_elevHead;
  if (count < 2) count = 2;

  for (uint8_t i = 0; i < count; i++) {
    float val = s_elevHistory[i];
    if (val < minAlt) minAlt = val;
    if (val > maxAlt) maxAlt = val;
  }
  if (maxAlt - minAlt < 5.0f) maxAlt = minAlt + 5.0f;

  int innerX = b.x + 8;
  int innerY = b.y + 20;
  int innerW = b.w - 16;
  int innerH = b.h - 26;

  tft.fillRect(innerX, innerY, innerW, innerH, COLOR_HERO_BG);
  tft.drawRect(innerX, innerY, innerW, innerH, COLOR_CARD_ACC);

  int prevPx = -1, prevPy = -1;
  for (uint8_t i = 0; i < count; i++) {
    uint8_t idx = s_elevFilled ? ((s_elevHead + i) % ELEV_SAMPLES) : i;
    float val = s_elevHistory[idx];

    int px = innerX + (i * innerW) / (ELEV_SAMPLES - 1);
    int py = innerY + innerH - (int)(((val - minAlt) / (maxAlt - minAlt)) * (innerH - 4));

    if (prevPx != -1) {
      tft.drawLine(prevPx, prevPy, px, py, COLOR_GREEN);
      tft.drawLine(prevPx, prevPy + 1, px, py + 1, COLOR_GREEN);
    }
    prevPx = px;
    prevPy = py;
  }
}

// 13. CLOCK WIDGET
static void renderWidgetClock(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD_ACC);
  }
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 6);
  tft.print("CLOCK");

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 24);
  tft.print("GPS TIME");
}

// 14. BATTERY WIDGET
static void renderWidgetBattery(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, COLOR_CARD_ACC);
  }
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 6);
  tft.print("BATTERY");

  tft.setTextSize(2);
  tft.setTextColor(COLOR_GREEN, COLOR_CARD);
  tft.setCursor(b.x + 6, b.y + 24);
  tft.setTextPadding(b.w - 12);
  tft.printf("%u%%", state.battery_pct);
  tft.setTextPadding(0);
}

// 15. GPS DIAGNOSTICS WIDGET
static void renderWidgetGpsDiagnostics(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 8, COLOR_CARD_ACC);
  }
  int y = b.y + 12;
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 10, y);
  tft.print("GPS FIX STATUS:");
  tft.setCursor(b.x + 110, y);
  tft.setTextColor(state.gps_has_fix ? COLOR_GREEN : COLOR_AMBER, COLOR_CARD);
  tft.setTextPadding(b.w - 120);
  tft.print(state.gps_has_fix ? "3D FIX VALID" : "SEARCHING...");
  tft.setTextPadding(0);
  y += 24;

  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 10, y);
  tft.print("SATELLITES:");
  tft.setCursor(b.x + 110, y);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD);
  tft.setTextPadding(b.w - 120);
  char satStr[32];
  snprintf(satStr, sizeof(satStr), "%u sats (HDOP: %.2f)", state.satellites, state.hdop);
  tft.print(satStr);
  tft.setTextPadding(0);
  y += 24;

  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 10, y);
  tft.print("COORDINATES:");
  tft.setCursor(b.x + 110, y);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  tft.setTextPadding(b.w - 120);
  char coordStr[32];
  snprintf(coordStr, sizeof(coordStr), "%.5f, %.5f", state.lat, state.lon);
  tft.print(coordStr);
  tft.setTextPadding(0);
}

// 16. NMEA CONSOLE WIDGET
static void renderWidgetNmeaConsole(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, 42, 6, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, 42, 6, COLOR_CARD_ACC);

    tft.fillRoundRect(b.x, b.y + 46, b.w, b.h - 46, 6, COLOR_CONSOLE);
    tft.drawRoundRect(b.x, b.y + 46, b.w, b.h - 46, 6, COLOR_GREEN);
  }
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 8, b.y + 6);
  tft.print("PORT:");
  tft.setTextColor(COLOR_CYAN, COLOR_CARD);
  tft.printf(" RX:%u 115200", g_gps_debug.active_rx_pin);

  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 120, b.y + 6);
  tft.print("STATUS:");
  tft.setTextColor(state.gps_has_fix ? COLOR_GREEN : COLOR_AMBER, COLOR_CARD);
  tft.setTextPadding(b.x + b.w - (b.x + 168)); // "3D FIX" (6ch) vs "SEARCHING" (9ch)
  tft.print(state.gps_has_fix ? "3D FIX" : "SEARCHING");
  tft.setTextPadding(0);

  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 8, b.y + 24);
  tft.setTextPadding(b.w - 16); // Chars/Sentences counters grow digits over a ride
  char statsStr[36];
  snprintf(statsStr, sizeof(statsStr), "Chars: %lu | Sentences: %lu",
           (unsigned long)g_gps_debug.total_chars, (unsigned long)g_gps_debug.sentences_passed);
  tft.print(statsStr);
  tft.setTextPadding(0);

  // Terminal box — cleared every frame (not just on force) since each of the
  // scrolling NMEA lines below has its own independently-varying length.
  int termY = b.y + 50;
  int termH = b.h - 54;
  tft.fillRect(b.x + 4, termY, b.w - 8, termH, COLOR_CONSOLE);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREEN, COLOR_CONSOLE);

  if (g_gps_debug.total_chars == 0) {
    tft.setTextColor(COLOR_AMBER, COLOR_CONSOLE);
    tft.setCursor(b.x + 8, termY + 12);
    tft.print("WAITING FOR UART BYTES...");
  } else {
    int y = termY + 4;
    for (int i = 0; i < NMEA_BUFFER_LINES; i++) {
      int lineIdx = (g_gps_debug.line_head + i) % NMEA_BUFFER_LINES;
      if (strlen(g_gps_debug.last_lines[lineIdx]) > 0) {
        tft.setCursor(b.x + 6, y);
        tft.print(g_gps_debug.last_lines[lineIdx]);
        y += 24;
      }
    }
  }
}

// 17. BLE MANAGER WIDGET
static void renderWidgetBleManager(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 8, COLOR_CARD_ACC);
  }
  // Scan Button (y: b.y + 6 .. b.y + 34)
  uint16_t scanBtnColor = g_ble_scanning ? COLOR_AMBER : COLOR_CYAN;
  tft.fillRoundRect(b.x + 6, b.y + 6, b.w - 12, 28, 6, scanBtnColor);
  tft.setTextColor(TFT_BLACK, scanBtnColor);
  tft.setTextSize(1);
  tft.setCursor(b.x + 50, b.y + 16);
  tft.print(g_ble_scanning ? "SCANNING BLE..." : "SCAN & ADD SENSORS");

  int y = b.y + 46;

  // CSC
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 10, y);
  tft.print("SPEED/CAD:");
  tft.setCursor(b.x + 88, y);
  tft.setTextPadding(b.w - 98);
  if (g_settings.paired_csc_mac[0] != '\0') {
    tft.setTextColor(COLOR_GREEN, COLOR_CARD);
    tft.printf("%.10s..", g_settings.paired_csc_mac);
    tft.fillRoundRect(b.x + b.w - 60, y - 4, 52, 18, 4, COLOR_RED);
    tft.setTextColor(TFT_WHITE, COLOR_RED);
    tft.setCursor(b.x + b.w - 54, y);
    tft.print("FORGET");
  } else {
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.print("NOT PAIRED");
  }
  tft.setTextPadding(0);
  y += 30;

  // HR
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 10, y);
  tft.print("HEART RATE:");
  tft.setCursor(b.x + 88, y);
  tft.setTextPadding(b.w - 98);
  if (g_settings.paired_hr_mac[0] != '\0') {
    tft.setTextColor(COLOR_GREEN, COLOR_CARD);
    tft.printf("%.10s..", g_settings.paired_hr_mac);
    tft.fillRoundRect(b.x + b.w - 60, y - 4, 52, 18, 4, COLOR_RED);
    tft.setTextColor(TFT_WHITE, COLOR_RED);
    tft.setCursor(b.x + b.w - 54, y);
    tft.print("FORGET");
  } else {
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.print("NOT PAIRED");
  }
  tft.setTextPadding(0);
  y += 30;

  // POWER
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 10, y);
  tft.print("POWER METER:");
  tft.setCursor(b.x + 88, y);
  tft.setTextPadding(b.w - 98);
  if (g_settings.paired_power_mac[0] != '\0') {
    tft.setTextColor(COLOR_GREEN, COLOR_CARD);
    tft.printf("%.10s..", g_settings.paired_power_mac);
    tft.fillRoundRect(b.x + b.w - 60, y - 4, 52, 18, 4, COLOR_RED);
    tft.setTextColor(TFT_WHITE, COLOR_RED);
    tft.setCursor(b.x + b.w - 54, y);
    tft.print("FORGET");
  } else {
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.print("NOT PAIRED");
  }
  tft.setTextPadding(0);
  y += 34;

  // Separator line
  tft.drawFastHLine(b.x + 8, y, b.w - 16, COLOR_CARD_ACC);
  y += 10;

  // GPS Diagnostics Summary
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 10, y);
  tft.print("GPS FIX STATUS:");
  tft.setCursor(b.x + 110, y);
  tft.setTextColor(state.gps_has_fix ? COLOR_GREEN : COLOR_AMBER, COLOR_CARD);
  tft.setTextPadding(b.w - 120);
  tft.print(state.gps_has_fix ? "3D FIX VALID" : "SEARCHING...");
  tft.setTextPadding(0);
  y += 22;

  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 10, y);
  tft.print("SATELLITES:");
  tft.setCursor(b.x + 110, y);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD);
  tft.setTextPadding(b.w - 120);
  char satStr[32];
  snprintf(satStr, sizeof(satStr), "%u sats (HDOP: %.2f)", state.satellites, state.hdop);
  tft.print(satStr);
  tft.setTextPadding(0);
  y += 22;

  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 10, y);
  tft.print("COORDINATES:");
  tft.setCursor(b.x + 110, y);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  tft.setTextPadding(b.w - 120);
  char coordStr[32];
  snprintf(coordStr, sizeof(coordStr), "%.5f, %.5f", state.lat, state.lon);
  tft.print(coordStr);
  tft.setTextPadding(0);
}

static bool touchWidgetBleManager(const Rect& b, int16_t x, int16_t y) {
  if (x >= b.x + 6 && x <= b.x + b.w - 6 && y >= b.y + 6 && y <= b.y + 34) {
    triggerBleScan();
    return true;
  }
  int rowY = b.y + 46;
  if (x >= b.x + b.w - 60 && x <= b.x + b.w - 8 && y >= rowY - 4 && y <= rowY + 16) {
    forgetSensorProfile(BLE_PROFILE_CSC);
    return true;
  }
  rowY += 30;
  if (x >= b.x + b.w - 60 && x <= b.x + b.w - 8 && y >= rowY - 4 && y <= rowY + 16) {
    forgetSensorProfile(BLE_PROFILE_HR);
    return true;
  }
  rowY += 30;
  if (x >= b.x + b.w - 60 && x <= b.x + b.w - 8 && y >= rowY - 4 && y <= rowY + 16) {
    forgetSensorProfile(BLE_PROFILE_POWER);
    return true;
  }
  return false;
}

// 18. SETTINGS LIST WIDGET
static void renderWidgetSettingsList(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, COLOR_CARD);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 8, COLOR_CARD_ACC);
  }

  int y = b.y + 14;
  tft.setTextSize(1);

  // Row 1: Units Toggle
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 10, y);
  tft.print("SPEED UNITS:");
  tft.fillRoundRect(b.x + 106, y - 4, 116, 24, 4, COLOR_CYAN);
  tft.setTextColor(TFT_BLACK, COLOR_CYAN);
  tft.setCursor(b.x + 116, y + 3);
  tft.print(g_settings.units == 0 ? "METRIC (KM/H)" : "IMPERIAL (MPH)");
  y += 36;

  // Row 2: Screen Brightness Control
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 10, y);
  tft.print("BRIGHTNESS:");
  tft.fillRoundRect(b.x + 106, y - 4, 116, 24, 4, COLOR_CARD_ACC);
  tft.setTextColor(TFT_WHITE, COLOR_CARD_ACC);
  tft.setCursor(b.x + 140, y + 3);
  char brightStr[16];
  snprintf(brightStr, sizeof(brightStr), "%u%%", (g_settings.brightness * 100) / 255);
  tft.print(brightStr);
  y += 36;

  // Row 3: Wheel Size (700x25c)
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 10, y);
  tft.print("WHEEL SIZE:");
  tft.setCursor(b.x + 106, y);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  char wheelStr[24];
  snprintf(wheelStr, sizeof(wheelStr), "%u mm (700x25c)", g_settings.wheel_circumference_mm);
  tft.print(wheelStr);
  y += 36;

  // Row 4: SD Logging (GPX)
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 10, y);
  tft.print("SD LOGGING:");
  tft.setCursor(b.x + 106, y);
  tft.setTextColor(g_settings.sd_logging_enabled ? COLOR_GREEN : COLOR_AMBER, COLOR_CARD);
  tft.print(g_settings.sd_logging_enabled ? "ENABLED (GPX 1.1)" : "DISABLED");
  y += 36;

  // Row 5: Battery Voltage & Percentage
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 10, y);
  tft.print("BATTERY LEVEL:");
  tft.setCursor(b.x + 106, y);
  tft.setTextColor(COLOR_GREEN, COLOR_CARD);
  tft.setTextPadding(b.w - 116);
  char batStr[24];
  float vBat = readBatteryVoltage();
  snprintf(batStr, sizeof(batStr), "%u%% (%.2f V)", state.battery_pct, vBat);
  tft.print(batStr);
  tft.setTextPadding(0);
  y += 36;

  // Separator line
  tft.drawFastHLine(b.x + 10, y - 8, b.w - 20, COLOR_CARD_ACC);

  // Firmware Info
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(b.x + 10, y);
  tft.print("FIRMWARE:");
  tft.setCursor(b.x + 106, y);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD);
  tft.print("OpenCyclo v0.1.0-TreeUI");
}

static bool touchWidgetSettingsList(const Rect& b, int16_t x, int16_t y) {
  int rowY = b.y + 14;
  if (y >= rowY - 4 && y <= rowY + 22) {
    g_settings.units = (g_settings.units == 0) ? 1 : 0;
    saveSettings();
    return true;
  }
  rowY += 36;
  if (y >= rowY - 4 && y <= rowY + 22) {
    if (g_settings.brightness >= 250) g_settings.brightness = 50;
    else g_settings.brightness += 50;
    setDisplayBrightness(g_settings.brightness);
    saveSettings();
    return true;
  }
  rowY += 72; // Jump to SD Logging row
  if (y >= rowY - 4 && y <= rowY + 22) {
    g_settings.sd_logging_enabled = !g_settings.sd_logging_enabled;
    saveSettings();
    return true;
  }
  return false;
}

// Registry Table
static const WidgetDescriptor s_descriptors[WIDGET_TYPE_COUNT] = {
  {WIDGET_NONE, "None", "", "", "", nullptr, nullptr},
  {WIDGET_SPEED, "Speed", "SPEED", "KM/H", "MPH", renderWidgetSpeed, nullptr},
  {WIDGET_AVG_SPEED, "Avg Speed", "AVG SPD", "KM/H", "MPH", renderWidgetAvgSpeed, nullptr},
  {WIDGET_MAX_SPEED, "Max Speed", "MAX SPD", "KM/H", "MPH", renderWidgetMaxSpeed, nullptr},
  {WIDGET_DISTANCE, "Distance", "DIST", "KM", "MI", renderWidgetDistance, nullptr},
  {WIDGET_RIDE_TIME, "Ride Time", "TIME", "", "", renderWidgetRideTime, nullptr},
  {WIDGET_CADENCE, "Cadence", "CAD", "RPM", "RPM", renderWidgetCadence, nullptr},
  {WIDGET_HEART_RATE, "Heart Rate", "HR", "BPM", "BPM", renderWidgetHeartRate, nullptr},
  {WIDGET_POWER, "Power", "PWR", "W", "W", renderWidgetPower, nullptr},
  {WIDGET_ALTITUDE, "Altitude", "ALT", "M", "FT", renderWidgetAltitude, nullptr},
  {WIDGET_GRADE, "Grade", "GRADE", "%", "%", renderWidgetGrade, nullptr},
  {WIDGET_TOTAL_ASCENT, "Total Ascent", "ASCENT", "M", "FT", renderWidgetTotalAscent, nullptr},
  {WIDGET_ELEVATION_CHART, "Elevation Chart", "ELEV PROFILE", "", "", renderWidgetElevationChart, nullptr},
  {WIDGET_CLOCK, "Clock", "CLOCK", "", "", renderWidgetClock, nullptr},
  {WIDGET_BATTERY, "Battery", "BATTERY", "%", "%", renderWidgetBattery, nullptr},
  {WIDGET_GPS_DIAGNOSTICS, "GPS Diagnostics", "GPS", "", "", renderWidgetGpsDiagnostics, nullptr},
  {WIDGET_NMEA_CONSOLE, "NMEA Console", "NMEA", "", "", renderWidgetNmeaConsole, nullptr},
  {WIDGET_BLE_MANAGER, "BLE Manager", "SENSORS", "", "", renderWidgetBleManager, touchWidgetBleManager},
  {WIDGET_SETTINGS_LIST, "Settings List", "SETTINGS", "", "", renderWidgetSettingsList, touchWidgetSettingsList}
};

void initWidgetRegistry() {
  Serial.println("[WIDGET REGISTRY] Initialized 18 modular widgets.");
}

const WidgetDescriptor* getWidgetDescriptor(WidgetType type) {
  if (type < WIDGET_TYPE_COUNT) {
    return &s_descriptors[type];
  }
  return &s_descriptors[WIDGET_NONE];
}

void renderWidget(WidgetType type, const Rect& bounds, const TelemetryState& state, bool forceFullRedraw) {
  if (type < WIDGET_TYPE_COUNT && s_descriptors[type].render_fn != nullptr) {
    s_descriptors[type].render_fn(bounds, state, forceFullRedraw);
  }
}

bool handleWidgetTouch(WidgetType type, const Rect& bounds, int16_t x, int16_t y) {
  if (type < WIDGET_TYPE_COUNT && s_descriptors[type].touch_fn != nullptr) {
    return s_descriptors[type].touch_fn(bounds, x, y);
  }
  return false;
}
