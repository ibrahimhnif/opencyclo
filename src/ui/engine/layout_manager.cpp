#include "layout_manager.h"
#include <stdio.h>

static uint16_t COLOR_BG       = TFT_BLACK;
static uint16_t COLOR_TEXT     = TFT_WHITE;
static uint16_t COLOR_LABEL    = tft.color565(102, 102, 102);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);

void renderPage(const PageConfig& page, uint8_t pageIdx, uint8_t totalPages, const TelemetryState& state, bool forceFullRedraw) {
  const TemplateSlotDefinition& slotDef = getTemplateDefinition(page.template_id);

  if (forceFullRedraw) {
    tft.fillScreen(COLOR_BG);

    tft.setFont(&fonts::FreeSans9pt7b);
    tft.setTextColor(COLOR_LABEL, COLOR_BG);
    tft.setCursor(6, 6);
    tft.print(page.title);

    if (totalPages > 1) {
      int startX = 120 - (totalPages * 8) / 2;
      for (int i = 0; i < totalPages; i++) {
        uint16_t dotColor = (i == pageIdx) ? COLOR_CYAN : COLOR_LABEL;
        tft.fillCircle(startX + (i * 8), 312, (i == pageIdx) ? 3 : 2, dotColor);
      }
    }
  }

  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(state.gps_has_fix ? COLOR_GREEN : COLOR_AMBER, COLOR_BG);
  tft.setCursor(6, 22);
  tft.setTextPadding(120);
  if (state.gps_has_fix) {
    tft.printf("gps 3d (%u)", state.satellites);
  } else {
    tft.print("gps search");
  }
  tft.setTextPadding(0);

  uint16_t stateColor = (state.ride_state == RIDE_STATE_ACTIVE) ? COLOR_GREEN :
                        ((state.ride_state == RIDE_STATE_PAUSED) ? COLOR_AMBER : COLOR_LABEL);
  tft.setTextColor(stateColor, COLOR_BG);
  tft.setCursor(150, 22);
  tft.setTextPadding(90);
  tft.printf("%s %u%%", (state.ride_state == RIDE_STATE_ACTIVE) ? "rec" :
                        ((state.ride_state == RIDE_STATE_PAUSED) ? "pause" : "stop"), state.battery_pct);
  tft.setTextPadding(0);

  uint8_t count = (page.widget_count < slotDef.max_slots) ? page.widget_count : slotDef.max_slots;
  for (uint8_t i = 0; i < count; i++) {
    WidgetType wType = page.widgets[i];
    if (wType != WIDGET_NONE) {
      renderWidget(wType, slotDef.slots[i], state, forceFullRedraw);
    }
  }

  if (slotDef.has_action_button) {
    const Rect& btn = slotDef.action_button_rect;
    uint16_t btnColor = (state.ride_state == RIDE_STATE_ACTIVE) ? COLOR_AMBER : COLOR_GREEN;
    tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 8, btnColor);
    tft.setFont(&fonts::FreeSans12pt7b);
    tft.setTextColor(TFT_BLACK, btnColor);
    tft.setCursor(btn.x + 50, btn.y + 16);
    if (state.ride_state == RIDE_STATE_ACTIVE) {
      tft.print("pause ride");
    } else if (state.ride_state == RIDE_STATE_PAUSED) {
      tft.print("resume ride");
    } else {
      tft.print("start ride");
    }
  }
}

bool handlePageTouch(const PageConfig& page, int16_t x, int16_t y) {
  const TemplateSlotDefinition& slotDef = getTemplateDefinition(page.template_id);

  if (slotDef.has_action_button) {
    const Rect& btn = slotDef.action_button_rect;
    if (x >= btn.x && x <= btn.x + btn.w && y >= btn.y && y <= btn.y + btn.h) {
      TelemetryState state = getTelemetrySnapshot();
      state.ride_state = (state.ride_state == RIDE_STATE_ACTIVE) ? RIDE_STATE_PAUSED : RIDE_STATE_ACTIVE;
      setTelemetryState(state);
      Serial.println("[UI] Ride state toggled via Action Button");
      return true;
    }
  }

  uint8_t count = (page.widget_count < slotDef.max_slots) ? page.widget_count : slotDef.max_slots;
  for (uint8_t i = 0; i < count; i++) {
    WidgetType wType = page.widgets[i];
    const Rect& r = slotDef.slots[i].rect;
    if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) {
      if (handleWidgetTouch(wType, r, x, y)) {
        return true;
      }
    }
  }
  return false;
}
