#include "ride_page.h"
#include "storage/settings.h"
#include <stdio.h>

static uint16_t COLOR_BG        = tft.color565(10, 14, 24);
static uint16_t COLOR_CARD      = tft.color565(20, 28, 44);
static uint16_t COLOR_CARD_ACC  = tft.color565(32, 44, 68);
static uint16_t COLOR_HERO_BG   = tft.color565(14, 22, 38);
static uint16_t COLOR_CYAN      = tft.color565(0, 210, 255);
static uint16_t COLOR_GREEN     = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER     = tft.color565(255, 171, 0);
static uint16_t COLOR_RED       = tft.color565(255, 71, 87);
static uint16_t COLOR_TEXT_MUT  = tft.color565(140, 155, 180);

void renderRidePage(const TelemetryState& state, bool forceFullRedraw) {
  if (forceFullRedraw) {
    tft.fillScreen(COLOR_BG);

    // 1. TOP HEADER STATUS BAR (y: 0 .. 24)
    tft.fillRect(0, 0, 240, 24, COLOR_CARD);
    tft.drawFastHLine(0, 24, 240, COLOR_CARD_ACC);

    // 2. HERO SPEED BOX (x: 4, y: 28, w: 232, h: 94)
    tft.fillRoundRect(4, 28, 232, 94, 8, COLOR_HERO_BG);
    tft.drawRoundRect(4, 28, 232, 94, 8, COLOR_CYAN);

    // 3. MIDDLE GRID CARDS (x: 4 & 122, y: 126, w: 114, h: 60)
    tft.fillRoundRect(4, 126, 114, 60, 6, COLOR_CARD);
    tft.drawRoundRect(4, 126, 114, 60, 6, COLOR_CARD_ACC);

    tft.fillRoundRect(122, 126, 114, 60, 6, COLOR_CARD);
    tft.drawRoundRect(122, 126, 114, 60, 6, COLOR_CARD_ACC);

    // 4. BOTTOM SENSOR TRIPLE CARDS (x: 4, 83, 162, y: 190, w: 74, h: 54)
    tft.fillRoundRect(4, 190, 74, 54, 6, COLOR_CARD);
    tft.drawRoundRect(4, 190, 74, 54, 6, COLOR_CARD_ACC);

    tft.fillRoundRect(83, 190, 74, 54, 6, COLOR_CARD);
    tft.drawRoundRect(83, 190, 74, 54, 6, COLOR_CARD_ACC);

    tft.fillRoundRect(162, 190, 74, 54, 6, COLOR_CARD);
    tft.drawRoundRect(162, 190, 74, 54, 6, COLOR_CARD_ACC);

    // 5. BOTTOM PAGE INDICATOR DOTS (y: 310)
    for (int i = 0; i < 5; i++) {
      uint16_t dotColor = (i == 0) ? COLOR_CYAN : COLOR_CARD_ACC;
      tft.fillCircle(104 + (i * 8), 310, (i == 0) ? 3 : 2, dotColor);
    }
  }

  // --- TOP STATUS BAR ---
  tft.setTextSize(1);
  tft.setTextColor(state.gps_has_fix ? COLOR_GREEN : COLOR_AMBER, COLOR_CARD);
  tft.setCursor(6, 7);
  if (state.gps_has_fix) {
    tft.printf("GPS 3D (%u SAT)", state.satellites);
  } else {
    tft.print("GPS SEARCH");
  }

  uint16_t stateColor = (state.ride_state == RIDE_STATE_ACTIVE) ? COLOR_GREEN :
                        ((state.ride_state == RIDE_STATE_PAUSED) ? COLOR_AMBER : COLOR_TEXT_MUT);
  tft.fillCircle(168, 12, 3, stateColor);
  tft.setTextColor(stateColor, COLOR_CARD);
  tft.setCursor(176, 7);
  tft.printf("%s %u%%", (state.ride_state == RIDE_STATE_ACTIVE) ? "REC" :
                        ((state.ride_state == RIDE_STATE_PAUSED) ? "PAUSE" : "STOP"), state.battery_pct);

  // --- 1. HERO SPEED SECTION ---
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_HERO_BG);
  tft.setTextSize(1);
  tft.setCursor(12, 34);
  tft.print("SPEED");

  tft.setCursor(170, 34);
  tft.setTextColor(state.speed_source == SPEED_SOURCE_BLE_CSC ? COLOR_CYAN : COLOR_TEXT_MUT, COLOR_HERO_BG);
  tft.print(state.speed_source == SPEED_SOURCE_BLE_CSC ? "[BLE CSC]" : "[GPS]");

  tft.setTextColor(TFT_WHITE, COLOR_HERO_BG);
  tft.setTextSize(4);
  char speedBuf[12];
  float displaySpeed = (g_settings.units == 1) ? (state.speed_kmh * 0.621371f) : state.speed_kmh;
  snprintf(speedBuf, sizeof(speedBuf), "%4.1f", displaySpeed);
  tft.setCursor(14, 50);
  tft.print(speedBuf);

  tft.setTextSize(2);
  tft.setTextColor(COLOR_CYAN, COLOR_HERO_BG);
  tft.setCursor(174, 80);
  tft.print((g_settings.units == 1) ? "MPH" : "KM/H");

  // --- 2. MIDDLE GRID: DISTANCE & RIDE TIME ---
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(10, 132);
  tft.print(g_settings.units == 1 ? "DIST (MI)" : "DIST (KM)");

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  tft.setCursor(10, 152);
  char distBuf[12];
  float displayDist = (g_settings.units == 1) ? (state.trip_distance_km * 0.621371f) : state.trip_distance_km;
  snprintf(distBuf, sizeof(distBuf), "%.2f", displayDist);
  tft.print(distBuf);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(128, 132);
  tft.print("RIDE TIME");

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  tft.setCursor(128, 152);
  char timeBuf[12];
  uint32_t hrs = state.ride_time_s / 3600;
  uint32_t mins = (state.ride_time_s % 3600) / 60;
  uint32_t secs = state.ride_time_s % 60;
  if (hrs > 0) {
    snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u:%02u", hrs, mins, secs);
  } else {
    snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u", mins, secs);
  }
  tft.print(timeBuf);

  // --- 3. BOTTOM SENSOR TRIPLE CARDS ---
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(8, 196);
  tft.print("CAD(RPM)");
  tft.setTextSize(2);
  tft.setCursor(8, 216);
  if (state.cadence_rpm >= 0) {
    tft.setTextColor(COLOR_CYAN, COLOR_CARD);
    tft.printf("%d", state.cadence_rpm);
  } else {
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.print("--");
  }

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(87, 196);
  tft.print("HR (BPM)");
  tft.setTextSize(2);
  tft.setCursor(87, 216);
  if (state.heart_rate_bpm >= 0) {
    tft.setTextColor(COLOR_RED, COLOR_CARD);
    tft.printf("%d", state.heart_rate_bpm);
  } else {
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.print("--");
  }

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
  tft.setCursor(166, 196);
  tft.print("PWR (W)");
  tft.setTextSize(2);
  tft.setCursor(166, 216);
  if (state.power_watts >= 0) {
    tft.setTextColor(COLOR_GREEN, COLOR_CARD);
    tft.printf("%d", state.power_watts);
  } else {
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.print("--");
  }

  // --- 4. ACTION CONTROL BUTTON (y: 248 .. 298) ---
  uint16_t btnColor = (state.ride_state == RIDE_STATE_ACTIVE) ? COLOR_AMBER : COLOR_GREEN;
  tft.fillRoundRect(4, 248, 232, 50, 8, btnColor);
  tft.setTextColor(TFT_BLACK, btnColor);
  tft.setTextSize(2);
  tft.setCursor(48, 264);
  if (state.ride_state == RIDE_STATE_ACTIVE) {
    tft.print("PAUSE RIDE");
  } else if (state.ride_state == RIDE_STATE_PAUSED) {
    tft.print("RESUME RIDE");
  } else {
    tft.print("START RIDE");
  }
}

bool handleRidePageTouch(int16_t x, int16_t y) {
  // Touch Start / Pause / Resume Button (y: 248 .. 298)
  if (y >= 248 && y <= 298) {
    TelemetryState state = getTelemetrySnapshot();
    if (state.ride_state == RIDE_STATE_ACTIVE) {
      state.ride_state = RIDE_STATE_PAUSED;
      Serial.println("[UI] Ride PAUSED via touchscreen");
    } else {
      state.ride_state = RIDE_STATE_ACTIVE;
      Serial.println("[UI] Ride STARTED/RESUMED via touchscreen");
    }
    setTelemetryState(state);
    return true;
  }
  return false;
}
