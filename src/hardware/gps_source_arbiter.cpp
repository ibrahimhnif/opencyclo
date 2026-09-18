#include "gps_source_arbiter.h"
#include <cmath>
#include <algorithm>

static double haversineMeters(double lat1, double lon1, double lat2, double lon2) {
  const double r = 3.141592653589793 / 180.0;
  const double x = std::sin((lat2 - lat1) * r / 2);
  const double y = std::sin((lon2 - lon1) * r / 2);
  const double h = x * x + std::cos(lat1 * r) * std::cos(lat2 * r) * y * y;
  return 12742000.0 * std::asin(std::sqrt(std::min(1.0, std::max(0.0, h))));
}

GpsFixSource selectGpsSource(GpsSourceMode mode, bool hardwareValid,
                              bool phonePresent, uint32_t phoneAgeMs) {
  const bool phoneFresh = phonePresent && phoneAgeMs < PHONE_FIX_STALE_MS;
  if (mode == GPS_SOURCE_MODE_PHONE_FORCED) {
    return phoneFresh ? GPS_FIX_SOURCE_PHONE : GPS_FIX_SOURCE_NONE;
  }
  if (hardwareValid) return GPS_FIX_SOURCE_HARDWARE;
  return phoneFresh ? GPS_FIX_SOURCE_PHONE_FALLBACK : GPS_FIX_SOURCE_NONE;
}

PhoneSpeedResult computePhoneSpeedKmh(double prevLat, double prevLon, uint32_t prevAtMs,
                                       double lat, double lon, uint32_t atMs) {
  PhoneSpeedResult out{false, 0.0f};
  if (atMs <= prevAtMs) return out;
  const uint32_t dtMs = atMs - prevAtMs;
  if (dtMs > 10000) return out;  // gap too large for a meaningful instantaneous speed
  const double meters = haversineMeters(prevLat, prevLon, lat, lon);
  const float speedKmh = (float)(meters / (dtMs / 1000.0) * 3.6);
  if (!std::isfinite(speedKmh) || speedKmh < 0 || speedKmh > 120) return out;
  out.speedValid = true;
  out.speedKmh = speedKmh;
  return out;
}
