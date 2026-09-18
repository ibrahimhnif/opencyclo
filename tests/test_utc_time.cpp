#include "core/utc_time.h"
#include <cassert>
#include <cstdio>

int main() {
  // 1970-01-01T00:00:00Z
  auto epoch = utc::civilFromEpoch(0);
  assert(epoch.year == 1970 && epoch.month == 1 && epoch.day == 1);
  assert(epoch.hour == 0 && epoch.minute == 0 && epoch.second == 0);

  // 2026-09-18T11:59:30Z, spot-checked against a reference converter.
  auto known = utc::civilFromEpoch(1789732770u);
  assert(known.year == 2026 && known.month == 9 && known.day == 18);
  assert(known.hour == 11 && known.minute == 59 && known.second == 30);

  // Leap day: 2024-02-29T00:00:00Z.
  auto leap = utc::civilFromEpoch(1709164800u);
  assert(leap.year == 2024 && leap.month == 2 && leap.day == 29);

  // Non-leap century boundary: 2000-03-01T00:00:00Z (2000 IS a leap year --
  // divisible by 400 -- so Feb 29 exists; this checks the day right after).
  auto centuryLeap = utc::civilFromEpoch(951868800u);
  assert(centuryLeap.year == 2000 && centuryLeap.month == 3 && centuryLeap.day == 1);

  // Year rollover: 2025-12-31T23:59:59Z -> 2026-01-01T00:00:00Z, one second apart.
  auto beforeRollover = utc::civilFromEpoch(1767225599u);
  auto afterRollover = utc::civilFromEpoch(1767225600u);
  assert(beforeRollover.year == 2025 && beforeRollover.month == 12 && beforeRollover.day == 31);
  assert(beforeRollover.hour == 23 && beforeRollover.minute == 59 && beforeRollover.second == 59);
  assert(afterRollover.year == 2026 && afterRollover.month == 1 && afterRollover.day == 1);
  assert(afterRollover.hour == 0 && afterRollover.minute == 0 && afterRollover.second == 0);

  puts("UTC epoch-to-civil conversion tests passed");
}
