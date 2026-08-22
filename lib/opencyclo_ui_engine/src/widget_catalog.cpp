#include "widget_catalog.h"

#define SIZES(...) ((uint8_t)(__VA_ARGS__))
#define BIT(size) (1u << (size))

static const WidgetMeta s_catalog[WIDGET_TYPE_COUNT] = {
  {WIDGET_NONE,            "None",           "",         "",     "",      0},
  {WIDGET_SPEED,           "Speed",          "SPEED",    "km/h", "mph",   BIT(SIZE_HERO)},
  {WIDGET_AVG_SPEED,       "Avg Speed",      "avg spd",  "km/h", "mph",   BIT(SIZE_SMALL) | BIT(SIZE_MEDIUM)},
  {WIDGET_MAX_SPEED,       "Max Speed",      "max spd",  "km/h", "mph",   BIT(SIZE_SMALL) | BIT(SIZE_MEDIUM)},
  {WIDGET_DISTANCE,        "Distance",       "distance", "km",   "mi",    BIT(SIZE_MEDIUM)},
  {WIDGET_RIDE_TIME,       "Ride Time",      "ride time","",     "",      BIT(SIZE_MEDIUM)},
  {WIDGET_CADENCE,         "Cadence",        "cadence",  "rpm",  "rpm",   BIT(SIZE_SMALL)},
  {WIDGET_HEART_RATE,      "Heart Rate",     "heart",    "bpm",  "bpm",   BIT(SIZE_SMALL)},
  {WIDGET_POWER,           "Power",          "power",    "w",    "w",     BIT(SIZE_SMALL)},
  {WIDGET_ALTITUDE,        "Altitude",       "altitude", "m",    "ft",    BIT(SIZE_SMALL) | BIT(SIZE_MEDIUM)},
  {WIDGET_GRADE,           "Grade",          "grade",    "%",    "%",     BIT(SIZE_SMALL) | BIT(SIZE_MEDIUM)},
  {WIDGET_TOTAL_ASCENT,    "Total Ascent",   "ascent",   "m",    "ft",    BIT(SIZE_SMALL) | BIT(SIZE_MEDIUM)},
  {WIDGET_ELEVATION_CHART, "Elevation Chart","elevation","",     "",      BIT(SIZE_LARGE) | BIT(SIZE_FULL)},
  {WIDGET_BATTERY,         "Battery",        "battery",  "%",    "%",     BIT(SIZE_SMALL)},
  {WIDGET_BLE_MANAGER,     "BLE Manager",    "sensors",  "",     "",      BIT(SIZE_FULL)},
  {WIDGET_SETTINGS_LIST,   "Settings List",  "settings", "",     "",      BIT(SIZE_FULL)},
};

const WidgetMeta& getWidgetMeta(WidgetType type) {
  if (type < WIDGET_TYPE_COUNT) return s_catalog[type];
  return s_catalog[WIDGET_NONE];
}

bool widgetSupportsSize(WidgetType type, SizeClass size) {
  if (type >= WIDGET_TYPE_COUNT || size >= SIZE_CLASS_COUNT) return false;
  return (s_catalog[type].supported_sizes_mask & BIT(size)) != 0;
}
