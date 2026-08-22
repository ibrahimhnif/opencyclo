#include "layout_manager.h"
#include <stdio.h>

static uint16_t COLOR_BG       = TFT_BLACK;
static uint16_t COLOR_TEXT     = TFT_WHITE;
static uint16_t COLOR_LABEL    = tft.color565(102, 102, 102);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);

// ---------------------------------------------------------------------------
// Status header geometry — ONE row, three side-by-side segments.
//
// Every template's first content slot starts at y = 28 (see template_engine.cpp),
// so the whole header has to live above that. FreeSans9pt7b measures 18px tall
// in LovyanGFX, so a single line drawn at y = 5 occupies rows 5..22 and leaves a
// 5px margin before the content band. Two 18px lines would not fit, which is why
// the title, gps status and ride state share this one row.
//
// Horizontal bands on the 240px-wide panel, each sized to the widest string its
// segment can produce (FreeSans9pt7b): "sensors" = 63px, "gps 99" = 54px,
// "pause 100%" = 100px. The 4px gaps keep neighbouring padded erases apart.
//   title   x   4 .. 72   (68px)
//   gps     x  76 .. 134  (58px)
//   state   x 138 .. 240  (102px)
// ---------------------------------------------------------------------------
static const int16_t STATUS_ROW_Y   = 5;
static const int16_t STATUS_TITLE_X = 4;
static const int16_t STATUS_TITLE_W = 68;
static const int16_t STATUS_GPS_X   = 76;
static const int16_t STATUS_GPS_W   = 58;
static const int16_t STATUS_STATE_X = 138;
static const int16_t STATUS_STATE_W = 102;

void renderPage(const PageConfig& page, uint8_t pageIdx, uint8_t totalPages, const TelemetryState& state, bool forceFullRedraw) {
  const TemplateSlotDefinition& slotDef = getTemplateDefinition(page.template_id);

  if (forceFullRedraw) {
    tft.fillScreen(COLOR_BG);

    // Segment 1 of the status header: page title. Only redrawn on a page
    // change (the fillScreen above already cleared it); the gps segment's
    // padded erase below clips any over-long title at x=76 every frame, so a
    // long app-supplied title can never collide with the live segments.
    tft.setFont(&fonts::FreeSans9pt7b);
    tft.setTextColor(COLOR_LABEL, COLOR_BG);
    tft.setTextPadding(STATUS_TITLE_W);
    tft.drawString(page.title, STATUS_TITLE_X, STATUS_ROW_Y);
    tft.setTextPadding(0);

    if (totalPages > 1) {
      int startX = 120 - (totalPages * 8) / 2;
      for (int i = 0; i < totalPages; i++) {
        uint16_t dotColor = (i == pageIdx) ? COLOR_CYAN : COLOR_LABEL;
        tft.fillCircle(startX + (i * 8), 312, (i == pageIdx) ? 3 : 2, dotColor);
      }
    }
  }

  // Segment 2: gps fix / satellite count. Green when fixed, amber otherwise.
  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(state.gps_has_fix ? COLOR_GREEN : COLOR_AMBER, COLOR_BG);
  tft.setTextPadding(STATUS_GPS_W);
  if (state.gps_has_fix) {
    // Clamp the printed count so the string can never outgrow its band
    // (satellites is a uint8_t; a bogus 3-digit value would overflow it).
    char gpsBuf[10];
    snprintf(gpsBuf, sizeof(gpsBuf), "gps %u", (unsigned)(state.satellites > 99 ? 99 : state.satellites));
    tft.drawString(gpsBuf, STATUS_GPS_X, STATUS_ROW_Y);
  } else {
    tft.drawString("gps --", STATUS_GPS_X, STATUS_ROW_Y);
  }
  tft.setTextPadding(0);

  // Segment 3: ride state + battery, right-hand end of the same row.
  uint16_t stateColor = (state.ride_state == RIDE_STATE_ACTIVE) ? COLOR_GREEN :
                        ((state.ride_state == RIDE_STATE_PAUSED) ? COLOR_AMBER : COLOR_LABEL);
  tft.setTextColor(stateColor, COLOR_BG);
  tft.setTextPadding(STATUS_STATE_W);
  char stateBuf[16];
  snprintf(stateBuf, sizeof(stateBuf), "%s %u%%", (state.ride_state == RIDE_STATE_ACTIVE) ? "rec" :
                        ((state.ride_state == RIDE_STATE_PAUSED) ? "pause" : "stop"), state.battery_pct);
  tft.drawString(stateBuf, STATUS_STATE_X, STATUS_ROW_Y);
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
      // Hit-test on the visible rectangle, but hand the full slot to the
      // registry so it can enforce the widget/size-class contract.
      if (handleWidgetTouch(wType, slotDef.slots[i], x, y)) {
        return true;
      }
    }
  }
  return false;
}
