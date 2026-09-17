#ifndef OPENCYCLO_HARDWARE_GPS_SOURCE_ARBITER_H
#define OPENCYCLO_HARDWARE_GPS_SOURCE_ARBITER_H

#include <stdint.h>

// Which GPS source the user (via app or device menu) has selected.
enum GpsSourceMode : uint8_t {
  GPS_SOURCE_MODE_HARDWARE = 0,      // M10, auto-fallback to phone when no hardware fix
  GPS_SOURCE_MODE_PHONE_FORCED = 1,  // always phone; M10 ignored entirely
};

// Which source actually fed g_gps_queue this epoch (diagnostics + arbitration result).
enum GpsFixSource : uint8_t {
  GPS_FIX_SOURCE_HARDWARE = 0,
  GPS_FIX_SOURCE_PHONE_FALLBACK = 1,  // hardware mode, M10 has no fix, phone substituted
  GPS_FIX_SOURCE_PHONE = 2,           // phone-forced mode
  GPS_FIX_SOURCE_NONE = 3,            // no usable source this epoch
};

// A phone fix older than this (received over BLE characteristic 0x190A) is
// treated as absent -- covers BLE disconnects or the app being killed.
const uint32_t PHONE_FIX_STALE_MS = 5000;

// Pure decision: which source should feed the GPS queue this epoch.
// `phoneAgeMs` is ignored when `phonePresent` is false.
GpsFixSource selectGpsSource(GpsSourceMode mode, bool hardwareValid,
                              bool phonePresent, uint32_t phoneAgeMs);

struct PhoneSpeedResult {
  bool speedValid;
  float speedKmh;
};

// Speed-from-displacement for a phone-derived fix, since the 0x190A payload
// carries no speed field. `speedValid` is false when there's no usable
// previous sample, the clock didn't advance, or the gap is too large for a
// meaningful instantaneous speed (>= 10s).
PhoneSpeedResult computePhoneSpeedKmh(double prevLat, double prevLon, uint32_t prevAtMs,
                                       double lat, double lon, uint32_t atMs);

#endif  // OPENCYCLO_HARDWARE_GPS_SOURCE_ARBITER_H
