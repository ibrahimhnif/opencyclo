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
  if (bpm >= 0) {
    snprintf(buf, sizeof(buf), "%d", bpm);
  } else {
    snprintf(buf, sizeof(buf), "--");
  }
  copyLabel(out, "hr", buf, TONE_DANGER);
}

void modelPower(int16_t watts, TileModel& out) {
  char buf[8];
  if (watts >= 0) {
    snprintf(buf, sizeof(buf), "%d", watts);
  } else {
    snprintf(buf, sizeof(buf), "--");
  }
  copyLabel(out, "power", buf, TONE_WARNING);
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
