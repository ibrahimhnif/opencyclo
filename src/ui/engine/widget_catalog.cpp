#include "widget_catalog.h"

#define BIT(size) (1u << (size))

// NOTE: the `label`, `unit_metric` and `unit_imperial` strings below are NOT yet
// consumed by any rendering code — every render function in widget_registry.cpp
// still hardcodes its own literals. They exist for the future companion-app
// widget picker, which will need human-readable names for widgets it cannot
// draw. Until that consumer exists they must be kept in sync with the render
// functions' literals BY HAND; the values here mirror what each widget actually
// prints on-device today (bare metric name, with the unit split out into the
// unit_* columns). `name` is the title-case display name for that same picker
// and has no on-device counterpart.
static const WidgetMeta s_catalog[WIDGET_TYPE_COUNT] = {
  {WIDGET_NONE,            "None",           "",                 "",     "",      0},
  {WIDGET_SPEED,           "Speed",          "speed",            "km/h", "mph",   BIT(SIZE_HERO)},
  {WIDGET_AVG_SPEED,       "Avg Speed",      "avg spd",          "km/h", "mph",   BIT(SIZE_SMALL) | BIT(SIZE_MEDIUM)},
  {WIDGET_MAX_SPEED,       "Max Speed",      "max spd",          "km/h", "mph",   BIT(SIZE_SMALL) | BIT(SIZE_MEDIUM)},
  {WIDGET_DISTANCE,        "Distance",       "dist",             "km",   "mi",    BIT(SIZE_MEDIUM)},
  {WIDGET_RIDE_TIME,       "Ride Time",      "ride time",        "",     "",      BIT(SIZE_MEDIUM)},
  {WIDGET_CADENCE,         "Cadence",        "cadence",          "rpm",  "rpm",   BIT(SIZE_SMALL)},
  {WIDGET_HEART_RATE,      "Heart Rate",     "heart",            "bpm",  "bpm",   BIT(SIZE_SMALL)},
  {WIDGET_POWER,           "Power",          "power",            "w",    "w",     BIT(SIZE_SMALL)},
  {WIDGET_ALTITUDE,        "Altitude",       "alt",              "m",    "ft",    BIT(SIZE_SMALL) | BIT(SIZE_MEDIUM)},
  {WIDGET_GRADE,           "Grade",          "grade",            "%",    "%",     BIT(SIZE_SMALL) | BIT(SIZE_MEDIUM)},
  {WIDGET_TOTAL_ASCENT,    "Total Ascent",   "asc",              "m",    "ft",    BIT(SIZE_SMALL) | BIT(SIZE_MEDIUM)},
  {WIDGET_ELEVATION_CHART, "Elevation Chart","elevation profile","",     "",      BIT(SIZE_LARGE) | BIT(SIZE_FULL)},
  {WIDGET_BATTERY,         "Battery",        "battery",          "%",    "%",     BIT(SIZE_SMALL)},
  // The two full-page widgets draw multi-row screens rather than one labelled
  // tile, so there is no single rendered literal to mirror; these are picker
  // labels only.
  {WIDGET_BLE_MANAGER,     "BLE Manager",    "sensors",          "",     "",      BIT(SIZE_FULL)},
  {WIDGET_SETTINGS_LIST,   "Settings List",  "settings",         "",     "",      BIT(SIZE_FULL)},
};

const WidgetMeta& getWidgetMeta(WidgetType type) {
  if (type < WIDGET_TYPE_COUNT) return s_catalog[type];
  return s_catalog[WIDGET_NONE];
}

bool widgetSupportsSize(WidgetType type, SizeClass size) {
  if (type >= WIDGET_TYPE_COUNT || size >= SIZE_CLASS_COUNT) return false;
  return (s_catalog[type].supported_sizes_mask & BIT(size)) != 0;
}
