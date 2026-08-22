#include "layout_manager.h"
#include <stdio.h>

static uint16_t COLOR_BG       = tft.color565(10, 14, 24);
static uint16_t COLOR_CARD     = tft.color565(20, 28, 44);
static uint16_t COLOR_CARD_ACC = tft.color565(32, 44, 68);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_RED      = tft.color565(255, 71, 87);
static uint16_t COLOR_TEXT_MUT = tft.color565(140, 155, 180);

void renderPage(const PageConfig& page, uint8_t pageIdx, uint8_t totalPages, const TelemetryState& state, bool forceFullRedraw) {
  const TemplateSlotDefinition& slotDef = getTemplateDefinition(page.template_id);

  if (forceFullRedraw) {
    tft.fillScreen(COLOR_BG);

    // 1. TOP HEADER (y: 0 .. 24)
    tft.fillRect(0, 0, 240, 24, COLOR_CARD);
    tft.drawFastHLine(0, 24, 240, COLOR_CARD_ACC);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, COLOR_CARD);
    tft.setCursor(8, 7);
    tft.print(page.title);

    // 2. BOTTOM PAGE INDICATOR DOTS (y: 310)
    if (totalPages > 1) {
      int startX = 120 - (totalPages * 8) / 2;
      for (int i = 0; i < totalPages; i++) {
        uint16_t dotColor = (i == pageIdx) ? COLOR_CYAN : COLOR_CARD_ACC;
        tft.fillCircle(startX + (i * 8), 310, (i == pageIdx) ? 3 : 2, dotColor);
      }
    }
  }

  // --- TOP STATUS BAR UPDATES ---
  // NOTE: setTextPadding() widens the glyph-erase rect drawn by print()/printf()
  // beyond the new string's own width. Without it, LovyanGFX only clears pixels
  // covered by the string being printed *this* frame; a shorter string (fewer
  // satellite digits, "SEARCHING" vs "3D FIX", REC/PAUSE/STOP length changes)
  // leaves the previous, wider string's trailing pixels on screen. Always reset
  // padding to 0 right after so it doesn't bleed into the next unrelated print.
  tft.setTextSize(1);
  tft.setTextColor(state.gps_has_fix ? COLOR_GREEN : COLOR_AMBER, COLOR_CARD);
  tft.setCursor(6, 7);
  tft.setTextPadding(160); // status text region: x=6 up to the state dot at x=168
  if (state.gps_has_fix) {
    tft.printf("GPS 3D (%u)", state.satellites);
  } else {
    tft.print("GPS SEARCH");
  }
  tft.setTextPadding(0);

  uint16_t stateColor = (state.ride_state == RIDE_STATE_ACTIVE) ? COLOR_GREEN :
                        ((state.ride_state == RIDE_STATE_PAUSED) ? COLOR_AMBER : COLOR_TEXT_MUT);
  tft.fillCircle(168, 12, 3, stateColor);
  tft.setTextColor(stateColor, COLOR_CARD);
  tft.setCursor(176, 7);
  tft.setTextPadding(60); // "PAUSE 100%" is the longest string this field prints
  tft.printf("%s %u%%", (state.ride_state == RIDE_STATE_ACTIVE) ? "REC" :
                        ((state.ride_state == RIDE_STATE_PAUSED) ? "PAUSE" : "STOP"), state.battery_pct);
  tft.setTextPadding(0);

  // --- RENDER ASSIGNED WIDGETS ---
  uint8_t count = (page.widget_count < slotDef.max_slots) ? page.widget_count : slotDef.max_slots;
  for (uint8_t i = 0; i < count; i++) {
    WidgetType wType = page.widgets[i];
    if (wType != WIDGET_NONE) {
      renderWidget(wType, slotDef.slot_rects[i], state, forceFullRedraw);
    }
  }

  // --- ACTION BUTTON (IF TEMPLATE INCLUDES ONE) ---
  if (slotDef.has_action_button) {
    const Rect& b = slotDef.action_button_rect;
    uint16_t btnColor = (state.ride_state == RIDE_STATE_ACTIVE) ? COLOR_AMBER : COLOR_GREEN;
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, btnColor);
    tft.setTextColor(TFT_BLACK, btnColor);
    tft.setTextSize(2);
    tft.setCursor(b.x + 44, b.y + 16);
    if (state.ride_state == RIDE_STATE_ACTIVE) {
      tft.print("PAUSE RIDE");
    } else if (state.ride_state == RIDE_STATE_PAUSED) {
      tft.print("RESUME RIDE");
    } else {
      tft.print("START RIDE");
    }
  }
}

bool handlePageTouch(const PageConfig& page, int16_t x, int16_t y) {
  const TemplateSlotDefinition& slotDef = getTemplateDefinition(page.template_id);

  // Check Action Button Tap
  if (slotDef.has_action_button) {
    const Rect& b = slotDef.action_button_rect;
    if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h) {
      TelemetryState state = getTelemetrySnapshot();
      if (state.ride_state == RIDE_STATE_ACTIVE) {
        state.ride_state = RIDE_STATE_PAUSED;
        Serial.println("[UI] Ride PAUSED via Action Button");
      } else {
        state.ride_state = RIDE_STATE_ACTIVE;
        Serial.println("[UI] Ride STARTED/RESUMED via Action Button");
      }
      setTelemetryState(state);
      return true;
    }
  }

  // Check Touch on Assigned Widgets
  uint8_t count = (page.widget_count < slotDef.max_slots) ? page.widget_count : slotDef.max_slots;
  for (uint8_t i = 0; i < count; i++) {
    WidgetType wType = page.widgets[i];
    const Rect& r = slotDef.slot_rects[i];
    if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) {
      if (handleWidgetTouch(wType, r, x, y)) {
        return true;
      }
    }
  }
  return false;
}
