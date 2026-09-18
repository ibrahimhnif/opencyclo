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

// Which altitude source the user has selected. HARDWARE keeps the existing
// barometer-preferred/GPS-fallback behavior (fusion_task.cpp) and only adds
// the phone as a further fallback when neither is usable; PHONE_FORCED
// ignores the barometer and GPS altitude entirely.
enum AltitudeSourceMode : uint8_t {
  ALTITUDE_SOURCE_MODE_HARDWARE = 0,
  ALTITUDE_SOURCE_MODE_PHONE_FORCED = 1,
};

// Which heading source the user has selected. The device has no onboard
// magnetometer -- HARDWARE means "no compass heading published" (consumers
// keep using the existing GPS-direction-of-travel logic in
// navigation/position_heading.h, which is unrelated to this toggle);
// PHONE_FORCED publishes the phone's magnetometer heading when fresh.
enum HeadingSourceMode : uint8_t {
  HEADING_SOURCE_MODE_HARDWARE = 0,
  HEADING_SOURCE_MODE_PHONE_FORCED = 1,
};

// A phone altitude/heading sample older than this (received over BLE
// characteristics 0x190C/0x190D) is treated as absent.
const uint32_t PHONE_ALTITUDE_STALE_MS = 5000;
const uint32_t PHONE_HEADING_STALE_MS = 3000;

// Pure decision: should the phone altitude sample be used as a fallback,
// given the barometer/GPS altitude is already unusable this epoch.
bool shouldUsePhoneAltitude(AltitudeSourceMode mode, bool hardwareAltitudeValid,
                             bool phonePresent, uint32_t phoneAgeMs);

// Pure decision: should the phone heading sample be published this epoch.
bool shouldUsePhoneHeading(HeadingSourceMode mode, bool phonePresent, uint32_t phoneAgeMs);

struct PhoneSpeedResult {
  bool speedValid;
  float speedKmh;
};

// Speed-from-displacement for a phone-derived fix, since the 0x190A payload
// carries no speed field. `speedValid` is false when there's no usable
// previous sample, the clock didn't advance, or the gap is too large for a
// meaningful instantaneous speed (> 10s).
PhoneSpeedResult computePhoneSpeedKmh(double prevLat, double prevLon, uint32_t prevAtMs,
                                       double lat, double lon, uint32_t atMs);

#endif  // OPENCYCLO_HARDWARE_GPS_SOURCE_ARBITER_H
