#ifndef OPENCYCLO_CORE_UTC_TIME_H
#define OPENCYCLO_CORE_UTC_TIME_H

#include <stdint.h>

namespace utc {

struct Calendar {
  uint16_t year;
  uint8_t month, day, hour, minute, second;
};

// Pure, header-only: no libc gmtime()/timezone dependency, so it behaves
// identically on the ESP32 and under a host-compiled unit test. Howard
// Hinnant's civil_from_days algorithm, valid for any epochSeconds >= 0
// (proleptic Gregorian calendar, no leap seconds).
inline Calendar civilFromEpoch(uint32_t epochSeconds) {
  const int64_t days = epochSeconds / 86400;
  const uint32_t rem = epochSeconds % 86400;
  const int64_t z = days + 719468; // shift so day 0 is 0000-03-01
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const uint64_t doe = (uint64_t)(z - era * 146097);           // [0, 146096]
  const uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
  int64_t y = (int64_t)yoe + era * 400;
  const uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100); // [0, 365]
  const uint64_t mp = (5 * doy + 2) / 153;                      // [0, 11]
  const uint64_t d = doy - (153 * mp + 2) / 5 + 1;              // [1, 31]
  const uint64_t m = mp + (mp < 10 ? 3 : (uint64_t)-9);         // [1, 12]
  y += (int64_t)(m <= 2);

  Calendar out{};
  out.year = (uint16_t)y;
  out.month = (uint8_t)m;
  out.day = (uint8_t)d;
  out.hour = (uint8_t)(rem / 3600);
  out.minute = (uint8_t)((rem % 3600) / 60);
  out.second = (uint8_t)(rem % 60);
  return out;
}

} // namespace utc

#endif // OPENCYCLO_CORE_UTC_TIME_H
