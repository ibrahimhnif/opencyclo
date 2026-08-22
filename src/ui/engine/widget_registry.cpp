#include "widget_registry.h"
#include "widget_catalog.h"
#include "storage/settings.h"
#include <stdio.h>

static uint16_t COLOR_BG = TFT_BLACK;

static uint16_t COLOR_HAIRLINE = tft.color565(28, 28, 28);
static uint16_t COLOR_TEXT     = TFT_WHITE;
static uint16_t COLOR_LABEL    = tft.color565(102, 102, 102);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_RED      = tft.color565(255, 71, 87);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);

// 1. SPEED (hero) — the one widget with its own layout, everything else is a "tile."
static void renderWidgetSpeed(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
  }
  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(b.x + 4, b.y + 4);
  tft.setTextPadding(b.w - 8);
  tft.printf("speed . %s", state.speed_source == SPEED_SOURCE_BLE_CSC ? "ble" : "gps");
  tft.setTextPadding(0);

  tft.setFont(&fonts::FreeSans24pt7b);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  char buf[12];
  float speed = (g_settings.units == 1) ? (state.speed_kmh * 0.621371f) : state.speed_kmh;
  snprintf(buf, sizeof(buf), "%.1f", speed);
  tft.setCursor(b.x + 4, b.y + 24);
  tft.setTextPadding(b.w - 70);
  tft.print(buf);
  tft.setTextPadding(0);

  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(b.x + b.w - 44, b.y + b.h - 18);
  tft.print((g_settings.units == 1) ? "mph" : "km/h");
}

// Shared layout for every SMALL/MEDIUM "label above, value below, hairline
// above the tile" widget — same visual pattern, different label/value/color.
static void renderTile(const Rect& b, const char* label, const char* valueStr, uint16_t valueColor, bool force) {
  if (force) {
    tft.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
    tft.drawFastHLine(b.x, b.y, b.w, COLOR_HAIRLINE);
  }
  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(b.x + 4, b.y + 6);
  tft.setTextPadding(b.w - 8);
  tft.print(label);
  tft.setTextPadding(0);

  tft.setFont(&fonts::FreeSans12pt7b);
  tft.setTextColor(valueColor, COLOR_BG);
  tft.setCursor(b.x + 4, b.y + 22);
  tft.setTextPadding(b.w - 8);
  tft.print(valueStr);
  tft.setTextPadding(0);
}

// 2. DISTANCE
static void renderWidgetDistance(const Rect& b, const TelemetryState& state, bool force) {
  char buf[16];
  float d = (g_settings.units == 1) ? (state.trip_distance_km * 0.621371f) : state.trip_distance_km;
  snprintf(buf, sizeof(buf), "%.2f %s", d, (g_settings.units == 1) ? "mi" : "km");
  renderTile(b, "distance", buf, COLOR_TEXT, force);
}

// 3. RIDE TIME
static void renderWidgetRideTime(const Rect& b, const TelemetryState& state, bool force) {
  char buf[16];
  uint32_t hrs = state.ride_time_s / 3600;
  uint32_t mins = (state.ride_time_s % 3600) / 60;
  uint32_t secs = state.ride_time_s % 60;
  if (hrs > 0) {
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hrs, mins, secs);
  } else {
    snprintf(buf, sizeof(buf), "%02u:%02u", mins, secs);
  }
  renderTile(b, "ride time", buf, COLOR_TEXT, force);
}

// 4. CADENCE
static void renderWidgetCadence(const Rect& b, const TelemetryState& state, bool force) {
  char buf[8];
  if (state.cadence_rpm >= 0) snprintf(buf, sizeof(buf), "%d", state.cadence_rpm);
  else snprintf(buf, sizeof(buf), "--");
  renderTile(b, "cadence", buf, state.cadence_rpm >= 0 ? COLOR_CYAN : COLOR_LABEL, force);
}

// 5. HEART RATE
static void renderWidgetHeartRate(const Rect& b, const TelemetryState& state, bool force) {
  char buf[8];
  if (state.heart_rate_bpm >= 0) snprintf(buf, sizeof(buf), "%d", state.heart_rate_bpm);
  else snprintf(buf, sizeof(buf), "--");
  renderTile(b, "heart", buf, state.heart_rate_bpm >= 0 ? COLOR_RED : COLOR_LABEL, force);
}

// 6. POWER
static void renderWidgetPower(const Rect& b, const TelemetryState& state, bool force) {
  char buf[8];
  if (state.power_watts >= 0) snprintf(buf, sizeof(buf), "%d", state.power_watts);
  else snprintf(buf, sizeof(buf), "--");
  renderTile(b, "power", buf, state.power_watts >= 0 ? COLOR_GREEN : COLOR_LABEL, force);
}

// 7. ALTITUDE
static void renderWidgetAltitude(const Rect& b, const TelemetryState& state, bool force) {
  char buf[16];
  float alt = (g_settings.units == 1) ? (state.altitude_m * 3.28084f) : state.altitude_m;
  snprintf(buf, sizeof(buf), "%.0f %s", alt, (g_settings.units == 1) ? "ft" : "m");
  renderTile(b, "altitude", buf, COLOR_TEXT, force);
}

// 8. GRADE
static void renderWidgetGrade(const Rect& b, const TelemetryState& state, bool force) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%+.1f%%", state.grade_pct);
  uint16_t gradeColor = (state.grade_pct > 3.0f) ? COLOR_AMBER : ((state.grade_pct < -2.0f) ? COLOR_CYAN : COLOR_GREEN);
  renderTile(b, "grade", buf, gradeColor, force);
}

// 9. TOTAL ASCENT
static void renderWidgetTotalAscent(const Rect& b, const TelemetryState& state, bool force) {
  char buf[16];
  float asc = (g_settings.units == 1) ? (state.total_ascent_m * 3.28084f) : state.total_ascent_m;
  snprintf(buf, sizeof(buf), "%.0f %s", asc, (g_settings.units == 1) ? "ft" : "m");
  renderTile(b, "ascent", buf, COLOR_GREEN, force);
}

// 10. ELEVATION CHART
#define ELEV_SAMPLES 30
static float s_elevHistory[ELEV_SAMPLES];
static uint8_t s_elevHead = 0;
static bool s_elevFilled = false;

static void renderWidgetElevationChart(const Rect& b, const TelemetryState& state, bool force) {
  s_elevHistory[s_elevHead] = state.altitude_m;
  s_elevHead = (s_elevHead + 1) % ELEV_SAMPLES;
  if (s_elevHead == 0) s_elevFilled = true;

  if (force) {
    tft.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
    tft.drawFastHLine(b.x, b.y, b.w, COLOR_HAIRLINE);
    tft.setFont(&fonts::FreeSans9pt7b);
    tft.setTextColor(COLOR_LABEL, COLOR_BG);
    tft.setCursor(b.x + 4, b.y + 6);
    tft.print("elevation profile");
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

  int innerX = b.x + 4;
  int innerY = b.y + 26;
  int innerW = b.w - 8;
  int innerH = b.h - 32;
  tft.fillRect(innerX, innerY, innerW, innerH, COLOR_BG);

  int prevPx = -1, prevPy = -1;
  for (uint8_t i = 0; i < count; i++) {
    uint8_t idx = s_elevFilled ? ((s_elevHead + i) % ELEV_SAMPLES) : i;
    float val = s_elevHistory[idx];
    int px = innerX + (i * innerW) / (ELEV_SAMPLES - 1);
    int py = innerY + innerH - (int)(((val - minAlt) / (maxAlt - minAlt)) * (innerH - 4));
    if (prevPx != -1) {
      tft.drawLine(prevPx, prevPy, px, py, COLOR_GREEN);
    }
    prevPx = px;
    prevPy = py;
  }
}

// Stub renderer used for every widget until Tasks 6-10 replace it with the
// real Minimal-style implementation. Draws only the background fill so the
// dispatch/validation plumbing in this task is independently verifiable.
static void renderStub(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
  }
}

static const WidgetDescriptor s_descriptors[WIDGET_TYPE_COUNT] = {
  {WIDGET_NONE,            nullptr,              nullptr},
  {WIDGET_SPEED,           renderWidgetSpeed,    nullptr},
  {WIDGET_AVG_SPEED,       renderStub,           nullptr},
  {WIDGET_MAX_SPEED,       renderStub,           nullptr},
  {WIDGET_DISTANCE,        renderWidgetDistance, nullptr},
  {WIDGET_RIDE_TIME,       renderWidgetRideTime, nullptr},
  {WIDGET_CADENCE,         renderWidgetCadence,  nullptr},
  {WIDGET_HEART_RATE,      renderWidgetHeartRate,nullptr},
  {WIDGET_POWER,           renderWidgetPower,    nullptr},
  {WIDGET_ALTITUDE,        renderWidgetAltitude,       nullptr},
  {WIDGET_GRADE,           renderWidgetGrade,          nullptr},
  {WIDGET_TOTAL_ASCENT,    renderWidgetTotalAscent,    nullptr},
  {WIDGET_ELEVATION_CHART, renderWidgetElevationChart, nullptr},
  {WIDGET_BATTERY,         renderStub,           nullptr},
  {WIDGET_BLE_MANAGER,     renderStub,           nullptr},
  {WIDGET_SETTINGS_LIST,   renderStub,           nullptr},
};

void initWidgetRegistry() {
  Serial.println("[WIDGET REGISTRY] Initialized 15 modular widgets (Minimal style rebuild in progress).");
}

const WidgetDescriptor* getWidgetDescriptor(WidgetType type) {
  if (type < WIDGET_TYPE_COUNT) return &s_descriptors[type];
  return &s_descriptors[WIDGET_NONE];
}

void renderWidget(WidgetType type, const TemplateSlot& slot, const TelemetryState& state, bool forceFullRedraw) {
  if (type >= WIDGET_TYPE_COUNT) return;
  if (!widgetSupportsSize(type, slot.size_class)) {
    // Invalid widget/slot pairing (e.g. stale config): fail safe, don't draw garbage.
    if (forceFullRedraw) {
      tft.fillRect(slot.rect.x, slot.rect.y, slot.rect.w, slot.rect.h, COLOR_BG);
    }
    return;
  }
  const WidgetRenderFn fn = s_descriptors[type].render_fn;
  if (fn != nullptr) {
    fn(slot.rect, state, forceFullRedraw);
  }
}

bool handleWidgetTouch(WidgetType type, const Rect& bounds, int16_t x, int16_t y) {
  if (type < WIDGET_TYPE_COUNT && s_descriptors[type].touch_fn != nullptr) {
    return s_descriptors[type].touch_fn(bounds, x, y);
  }
  return false;
}
