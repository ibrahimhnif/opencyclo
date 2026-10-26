#ifndef OPENCYCLO_UI_ENGINE_WIDGET_MODEL_H
#define OPENCYCLO_UI_ENGINE_WIDGET_MODEL_H

// Pure view models: formatted text for dashboard widgets, with no drawing,
// no telemetry globals, and no settings globals. The firmware renderer and
// the browser preview both consume these, so text formatting is defined once.
// Unit-testable on the host (only <stdint.h>/<stddef.h>).

#include <stddef.h>
#include <stdint.h>

namespace wmodel {

// Value emphasis. The renderer maps a tone to a theme color; the model never
// names a color itself.
enum Tone : uint8_t {
  TONE_TEXT = 0,
  TONE_ACCENT,
  TONE_SUCCESS,
  TONE_WARNING,
  TONE_DANGER,
  TONE_MUTED,        // secondary text (label color)
  TONE_STYLE = 255,  // no emphasis: use the style's own foreground
};

struct TileModel {
  char label[24];
  char value[24];
  Tone tone;
};

struct HeroModel {
  char source[16];  // e.g. "speed . gps"
  char value[12];
  char unit[8];  // "km/h" or "mph"
};

// Speed source suffix for the hero label. Mirrors TelemetryState SpeedSource
// without including Arduino-dependent telemetry_state.h.
enum SpeedSourceHint : uint8_t {
  SPEED_HINT_NONE = 0,
  SPEED_HINT_GPS,
  SPEED_HINT_BLE,
};

enum RideStateHint : uint8_t {
  RIDE_HINT_IDLE = 0,
  RIDE_HINT_ACTIVE,
  RIDE_HINT_PAUSED,
};

// Dashboard bottom status line.
struct StatusModel {
  char gps[12];
  Tone gps_tone;
  char ride[8];
  Tone ride_tone;
  char battery[8];
  char page[8];
};

void modelSpeed(float displayKmh, bool imperial, SpeedSourceHint src, HeroModel& out);
void modelDistance(float tripKm, bool imperial, TileModel& out);
void modelRideTime(uint32_t rideTimeS, TileModel& out);
// rpm < 0 means disconnected; renders as 0 to match current panel behavior.
void modelCadence(int16_t rpm, TileModel& out);
void modelHeartRate(int16_t bpm, TileModel& out);
void modelPower(int16_t watts, TileModel& out);

// fixQuality 1 = weak fix; satellites clamp to 99, battery to 100%.
void modelStatus(bool fix, uint8_t fixQuality, uint8_t sats, RideStateHint ride,
                 uint8_t batteryPct, uint8_t pageIdx, uint8_t totalPages, StatusModel& out);

}  // namespace wmodel

#endif  // OPENCYCLO_UI_ENGINE_WIDGET_MODEL_H
