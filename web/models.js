// Shared view models for the browser tool. Formatting mirrors
// src/ui/engine/widget_model.cpp exactly (same precision, same labels) so
// the preview shows what the panel will show.

export const WIDGETS = [
  "none", "speed", "avg speed", "max speed", "distance", "ride time",
  "cadence", "heart rate", "power", "altitude", "grade", "total ascent",
  "elevation chart", "battery", "BLE manager", "settings list",
  "camera remote",
];

export const TEMPLATES = [
  "Hero + 6 grid",
  "4 grid",
  "2 grid + chart",
  "8 grid",
  "Full container",
];

export const DEFAULT_THEME = {
  schema: 1,
  bg: "#000000",
  panel: "#161A20",
  text: "#FFFFFF",
  label: "#A8B1BD",
  hairline: "#1C1C1C",
  accent: "#00D2FF",
  success: "#2ED573",
  warning: "#FFAB00",
  danger: "#FF6974",
  font_scale: 100,
};

export const THEME_KEYS = [
  ["bg", "Background"],
  ["panel", "Panel"],
  ["text", "Value text"],
  ["label", "Label text"],
  ["hairline", "Hairline"],
  ["accent", "Accent"],
  ["success", "Good"],
  ["warning", "Warn"],
  ["danger", "Bad"],
];

const MPH = 0.621371;

export function modelSpeed(displayKmh, imperial, source) {
  const v = imperial ? displayKmh * MPH : displayKmh;
  return {
    source: `speed . ${source === "ble" ? "ble" : "gps"}`,
    value: v.toFixed(1),
    unit: imperial ? "mph" : "km/h",
  };
}

export function modelDistance(tripKm, imperial) {
  const d = imperial ? tripKm * MPH : tripKm;
  return {
    label: imperial ? "dist (mi)" : "dist (km)",
    value: d.toFixed(2),
    tone: "text",
  };
}

export function modelRideTime(rideTimeS) {
  const hrs = Math.floor(rideTimeS / 3600);
  const mins = Math.floor((rideTimeS % 3600) / 60);
  const secs = rideTimeS % 60;
  const p2 = (n) => String(n).padStart(2, "0");
  return {
    label: "ride time",
    value: hrs > 0 ? `${p2(hrs)}:${p2(mins)}:${p2(secs)}` : `${p2(mins)}:${p2(secs)}`,
    tone: "text",
  };
}

// Negative sensor values mean disconnected; the panel draws them as 0.
function orZero(v) {
  return v >= 0 ? String(v) : "0";
}

const FEET = 3.28084;

export function modelTile(widgetId, sample) {
  const ft = (m) => (sample.imperial ? m * FEET : m).toFixed(0);
  switch (widgetId) {
    case 5:
      return modelRideTime(sample.rideTimeS);
    case 6:
      return { label: "cad", value: orZero(sample.cadence), tone: "accent" };
    case 7:
      return { label: "heart", value: orZero(sample.hr), tone: "danger" };
    case 8:
      return { label: "power", value: orZero(sample.power), tone: "success" };
    case 4:
      return modelDistance(sample.distanceKm, sample.imperial);
    case 2:
      return { label: "avg", value: (sample.imperial ? sample.avgKmh * MPH : sample.avgKmh).toFixed(1), tone: "text" };
    case 3:
      return { label: "max", value: (sample.imperial ? sample.maxKmh * MPH : sample.maxKmh).toFixed(1), tone: "text" };
    case 9:
      return { label: sample.imperial ? "alt (ft)" : "alt (m)", value: ft(sample.altM), tone: "text" };
    case 10: {
      const g = sample.gradePct;
      return {
        label: "grade",
        value: `${g >= 0 ? "+" : ""}${g.toFixed(1)}%`,
        tone: g > 3 ? "warning" : g < -2 ? "accent" : "success",
      };
    }
    case 11:
      return { label: sample.imperial ? "asc (ft)" : "asc (m)", value: ft(sample.ascentM), tone: "success" };
    case 13:
      return { label: "battery", value: `${sample.batteryPct}%`, tone: sample.batteryPct < 20 ? "warning" : "success" };
    default:
      return null;
  }
}

export const TONE_TO_THEME = {
  text: "text",
  accent: "accent",
  success: "success",
  warning: "warning",
  danger: "danger",
  muted: "label",
};

export function defaultSample() {
  return {
    speedKmh: 27.4,
    speedSource: "gps",
    imperial: false,
    distanceKm: 42.35,
    rideTimeS: 5425,
    cadence: 87,
    hr: 142,
    power: 183,
    avgKmh: 24.1,
    maxKmh: 52.8,
    altM: 318,
    gradePct: 4.5,
    ascentM: 612,
    batteryPct: 76,
  };
}
