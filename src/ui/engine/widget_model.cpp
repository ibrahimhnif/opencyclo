#include "ui/engine/widget_model.h"

#include <stdio.h>
#include <string.h>

namespace wmodel {

namespace {

void copyLabel(TileModel& out, const char* label, const char* value, Tone tone) {
  snprintf(out.label, sizeof(out.label), "%s", label);
  snprintf(out.value, sizeof(out.value), "%s", value);
  out.tone = tone;
}

}  // namespace

void modelSpeed(float displayKmh, bool imperial, SpeedSourceHint src, HeroModel& out) {
  const char* tag = (src == SPEED_HINT_BLE) ? "ble" : "gps";
  snprintf(out.source, sizeof(out.source), "speed . %s", tag);
  float v = imperial ? displayKmh * 0.621371f : displayKmh;
  snprintf(out.value, sizeof(out.value), "%.1f", v);
  snprintf(out.unit, sizeof(out.unit), "%s", imperial ? "mph" : "km/h");
}

void modelDistance(float tripKm, bool imperial, TileModel& out) {
  char buf[16];
  float d = imperial ? tripKm * 0.621371f : tripKm;
  snprintf(buf, sizeof(buf), "%.2f", d);
  copyLabel(out, imperial ? "dist (mi)" : "dist (km)", buf, TONE_TEXT);
}

void modelRideTime(uint32_t rideTimeS, TileModel& out) {
  char buf[16];
  uint32_t hrs = rideTimeS / 3600;
  uint32_t mins = (rideTimeS % 3600) / 60;
  uint32_t secs = rideTimeS % 60;
  if (hrs > 0) {
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hrs, mins, secs);
  } else {
    snprintf(buf, sizeof(buf), "%02u:%02u", mins, secs);
  }
  copyLabel(out, "ride time", buf, TONE_TEXT);
}

void modelCadence(int16_t rpm, TileModel& out) {
  char buf[8];
  // "cad" not "cadence": the SMALL tile fits the short label; see renderer.
  if (rpm >= 0) {
    snprintf(buf, sizeof(buf), "%d", rpm);
  } else {
    snprintf(buf, sizeof(buf), "0");
  }
  copyLabel(out, "cad", buf, TONE_ACCENT);
}

void modelHeartRate(int16_t bpm, TileModel& out) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", bpm >= 0 ? bpm : 0);
  copyLabel(out, "heart", buf, TONE_DANGER);
}

void modelPower(int16_t watts, TileModel& out) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", watts >= 0 ? watts : 0);
  copyLabel(out, "power", buf, TONE_SUCCESS);
}

// "avg"/"max" not "avg spd": the bold label must fit a SMALL tile's 66px budget.
void modelAvgSpeed(float kmh, bool imperial, TileModel& out) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%.1f", imperial ? kmh * 0.621371f : kmh);
  copyLabel(out, "avg", buf, TONE_TEXT);
}

void modelMaxSpeed(float kmh, bool imperial, TileModel& out) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%.1f", imperial ? kmh * 0.621371f : kmh);
  copyLabel(out, "max", buf, TONE_TEXT);
}

void modelAltitude(float meters, bool valid, bool imperial, TileModel& out) {
  char buf[16];
  if (valid) {
    snprintf(buf, sizeof(buf), "%.0f", imperial ? meters * 3.28084f : meters);
  } else {
    snprintf(buf, sizeof(buf), "--");
  }
  copyLabel(out, imperial ? "alt (ft)" : "alt (m)", buf, TONE_TEXT);
}

void modelGrade(float pct, TileModel& out) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%+.1f%%", pct);
  copyLabel(out, "grade", buf, pct > 3.0f ? TONE_WARNING : pct < -2.0f ? TONE_ACCENT : TONE_SUCCESS);
}

void modelTotalAscent(float meters, bool imperial, TileModel& out) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f", imperial ? meters * 3.28084f : meters);
  copyLabel(out, imperial ? "asc (ft)" : "asc (m)", buf, TONE_SUCCESS);
}

void modelBattery(uint8_t pct, TileModel& out) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%u%%", unsigned(pct));
  copyLabel(out, "battery", buf, pct < 20 ? TONE_WARNING : TONE_SUCCESS);
}

void modelCameraIndicator(int8_t batteryPct, bool charging, int8_t cardState, uint64_t freeBytes,
                          int8_t captureState, uint32_t captureSecs, const char* modeName,
                          CameraIndicatorModel& out) {
  out.has_battery = batteryPct >= 0;
  const unsigned pct = batteryPct > 100 ? 100u : batteryPct < 0 ? 0u : unsigned(batteryPct);
  out.battery[0] = '\0';
  if (out.has_battery) snprintf(out.battery, sizeof(out.battery), "%u%%", pct);
  out.battery_fill = uint8_t(pct);
  out.battery_tone = pct <= 15 ? TONE_DANGER : pct < 30 ? TONE_WARNING : TONE_SUCCESS;
  out.charging = out.has_battery && charging;

  out.storage_tone = TONE_DANGER;
  if (cardState < 0) {
    out.storage[0] = '\0';
    out.storage_tone = TONE_MUTED;
  } else if (cardState == 0) {
    const uint64_t gib = 1024ULL * 1024 * 1024;
    if (freeBytes < gib) {
      snprintf(out.storage, sizeof(out.storage), "SD %uM", unsigned(freeBytes / (1024 * 1024)));
      out.storage_tone = TONE_WARNING;
    } else {
      const double g = double(freeBytes) / double(gib);
      snprintf(out.storage, sizeof(out.storage), g < 100.0 ? "SD %.1fG" : "SD %.0fG", g);
      out.storage_tone = TONE_MUTED;
    }
  } else {
    snprintf(out.storage, sizeof(out.storage), "%s",
             cardState == 1 ? "No SD" : cardState == 2 ? "SD full" : "SD error");
  }

  if (captureState > 0) {
    const unsigned h = captureSecs / 3600, m = (captureSecs / 60) % 60, s = captureSecs % 60;
    if (h) snprintf(out.capture, sizeof(out.capture), "REC %u:%02u:%02u", h, m, s);
    else snprintf(out.capture, sizeof(out.capture), "REC %u:%02u", m, s);
    out.capture_tone = TONE_DANGER;
  } else {
    snprintf(out.capture, sizeof(out.capture), "%s", modeName ? modeName : "");
    out.capture_tone = TONE_MUTED;
  }
}

void modelStatus(bool fix, uint8_t fixQuality, uint8_t sats, RideStateHint ride,
                 uint8_t batteryPct, uint8_t pageIdx, uint8_t totalPages, StatusModel& out) {
  if (fix) {
    snprintf(out.gps, sizeof(out.gps), "GPS %u", unsigned(sats > 99 ? 99 : sats));
  } else {
    snprintf(out.gps, sizeof(out.gps), "%s", fixQuality == 1 ? "GPS weak" : "GPS --");
  }
  out.gps_tone = fix ? TONE_SUCCESS : TONE_WARNING;
  snprintf(out.ride, sizeof(out.ride), "%s",
           ride == RIDE_HINT_ACTIVE ? "REC" : ride == RIDE_HINT_PAUSED ? "PAUSE" : "IDLE");
  out.ride_tone = ride == RIDE_HINT_ACTIVE ? TONE_SUCCESS : ride == RIDE_HINT_PAUSED ? TONE_WARNING : TONE_MUTED;
  snprintf(out.battery, sizeof(out.battery), "%u%%", unsigned(batteryPct > 100 ? 100 : batteryPct));
  snprintf(out.page, sizeof(out.page), "%u/%u", unsigned(pageIdx + 1), unsigned(totalPages));
}

}  // namespace wmodel
