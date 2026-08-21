#include "ride_page.h"
#include <stdio.h>

static uint16_t COLOR_BG       = tft.color565(12, 16, 26);
static uint16_t COLOR_CARD     = tft.color565(26, 34, 52);
static uint16_t COLOR_CARD_ACC = tft.color565(36, 46, 68);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_RED      = tft.color565(255, 71, 87);
static uint16_t COLOR_TEXT_MUT = tft.color565(140, 155, 180);

void renderRidePage(const TelemetryState& state, bool forceFullRedraw) {
  if (forceFullRedraw) {
    tft.fillScreen(COLOR_BG);

    // Header Title
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.setTextSize(2);
    tft.setCursor(8, 6);
    tft.print("OPENCYCLO");

    // Hero Speed Card Container (x: 6, y: 28, w: 228, h: 98)
    tft.fillRoundRect(6, 28, 228, 98, 8, COLOR_CARD);
    tft.drawRoundRect(6, 28, 228, 98, 8, COLOR_CARD_ACC);

    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.setCursor(14, 34);
    tft.print("SPEED");

    tft.setCursor(180, 34);
    tft.print("KM/H");

    // Grid Container Tiles (4 tiles)
    // Tile 1: Dist (6, 160, 111, 56)
    tft.fillRoundRect(6, 160, 111, 56, 6, COLOR_CARD);
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.setCursor(12, 166);
    tft.print("DISTANCE");
    tft.setCursor(85, 166);
    tft.print("km");

    // Tile 2: Time (123, 160, 111, 56)
    tft.fillRoundRect(123, 160, 111, 56, 6, COLOR_CARD);
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.setCursor(129, 166);
    tft.print("RIDE TIME");

    // Tile 3: Avg Speed (6, 220, 111, 56)
    tft.fillRoundRect(6, 220, 111, 56, 6, COLOR_CARD);
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.setCursor(12, 226);
    tft.print("AVG SPEED");
    tft.setCursor(80, 226);
    tft.print("km/h");

    // Tile 4: Altitude (123, 220, 111, 56)
    tft.fillRoundRect(123, 220, 111, 56, 6, COLOR_CARD);
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.setCursor(129, 226);
    tft.print("ALTITUDE");
    tft.setCursor(205, 226);
    tft.print("m");
  }

  // Header GPS Status Indicator
  if (state.gps_has_fix) {
    tft.fillCircle(195, 12, 5, COLOR_GREEN);
    tft.setTextColor(COLOR_GREEN, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(204, 8);
    tft.print("GPS");
  } else {
    tft.fillCircle(195, 12, 5, COLOR_AMBER);
    tft.setTextColor(COLOR_AMBER, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(204, 8);
    tft.print("SEARCH");
  }

  // Hero Speed Display Value
  char speedStr[16];
  snprintf(speedStr, sizeof(speedStr), "%4.1f", state.speed_kmh);

  tft.setTextSize(5);
  tft.setTextColor(state.gps_has_fix ? COLOR_CYAN : TFT_WHITE, COLOR_CARD);
  tft.setCursor(14, 52);
  tft.print(speedStr);

  // Speed source badge inside hero card
  tft.setTextSize(1);
  tft.setCursor(170, 102);
  if (state.speed_source == SPEED_SOURCE_BLE_CSC) {
    tft.setTextColor(COLOR_GREEN, COLOR_CARD);
    tft.print("[CSC]");
  } else if (state.speed_source == SPEED_SOURCE_GPS) {
    tft.setTextColor(COLOR_CYAN, COLOR_CARD);
    tft.print("[GPS]");
  } else {
    tft.setTextColor(COLOR_TEXT_MUT, COLOR_CARD);
    tft.print("[---]");
  }

  // Ride State Banner & Button (y: 130 .. 154)
  uint16_t badgeColor = COLOR_CARD_ACC;
  const char* stateText = "IDLE";
  if (state.ride_state == RIDE_STATE_ACTIVE) {
    badgeColor = COLOR_GREEN;
    stateText = "RIDE ACTIVE";
  } else if (state.ride_state == RIDE_STATE_PAUSED) {
    badgeColor = COLOR_AMBER;
    stateText = "PAUSED";
  }

  tft.fillRoundRect(6, 130, 110, 24, 12, badgeColor);
  tft.setTextColor(TFT_WHITE, badgeColor);
  tft.setTextSize(1);
  tft.setCursor(16, 138);
  tft.print(stateText);

  // Manual Ride Control Button Target
  tft.fillRoundRect(123, 130, 111, 24, 12, (state.ride_state == RIDE_STATE_IDLE) ? COLOR_CYAN : COLOR_RED);
  tft.setTextColor(TFT_BLACK, (state.ride_state == RIDE_STATE_IDLE) ? COLOR_CYAN : COLOR_RED);
  tft.setTextSize(1);
  tft.setCursor(133, 138);
  if (state.ride_state == RIDE_STATE_IDLE) {
    tft.print("START RIDE");
  } else {
    tft.print("END RIDE");
  }

  // Tile 1 Value: Distance
  char distStr[16];
  snprintf(distStr, sizeof(distStr), "%.2f", state.trip_distance_km);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, COLOR_CARD);
  tft.setCursor(12, 186);
  tft.print(distStr);

  // Tile 2 Value: Ride Time
  uint32_t hrs = state.ride_time_s / 3600;
  uint32_t mins = (state.ride_time_s % 3600) / 60;
  uint32_t secs = state.ride_time_s % 60;
  char timeStr[16];
  if (hrs > 0) {
    snprintf(timeStr, sizeof(timeStr), "%02u:%02u:%02u", hrs, mins, secs);
  } else {
    snprintf(timeStr, sizeof(timeStr), "%02u:%02u", mins, secs);
  }
  tft.setTextSize(2);
  tft.setCursor(129, 186);
  tft.print(timeStr);

  // Tile 3 Value: Avg Speed
  char avgStr[16];
  snprintf(avgStr, sizeof(avgStr), "%.1f", state.avg_speed_kmh);
  tft.setTextSize(2);
  tft.setCursor(12, 246);
  tft.print(avgStr);

  // Tile 4 Value: Altitude
  char altStr[16];
  snprintf(altStr, sizeof(altStr), "%.0f", state.altitude_m);
  tft.setTextSize(2);
  tft.setCursor(129, 246);
  tft.print(altStr);
}

bool handleRidePageTouch(int16_t x, int16_t y) {
  // Check Start/End Ride button tap (x: 123..234, y: 130..154)
  if (x >= 123 && x <= 234 && y >= 130 && y <= 154) {
    TelemetryState current = getTelemetrySnapshot();
    if (current.ride_state == RIDE_STATE_IDLE) {
      current.ride_state = RIDE_STATE_ACTIVE;
      current.trip_distance_km = 0.0f;
      current.ride_time_s = 0;
      current.avg_speed_kmh = 0.0f;
      current.max_speed_kmh = 0.0f;
    } else {
      current.ride_state = RIDE_STATE_IDLE;
    }
    setTelemetryState(current);
    return true;
  }
  return false;
}
