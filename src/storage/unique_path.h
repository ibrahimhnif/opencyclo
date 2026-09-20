#ifndef OPENCYCLO_STORAGE_UNIQUE_PATH_H
#define OPENCYCLO_STORAGE_UNIQUE_PATH_H

#include <SD_MMC.h>
#include <stdint.h>
#include <stdio.h>

// Naming policy shared by every file the device creates on the SD card (ride
// logs, screenshots): a GPS-clock timestamp when one is available, uptime
// seconds otherwise, and never overwrite -- an existing name gets _1, _2, ...
namespace storage {

inline void ensureDir(const char* dir) {
  if (!SD_MMC.exists(dir)) SD_MMC.mkdir(dir);
}

// "<dir>/YYYYMMDD_HHMMSS" when the clock is set (year >= 2020), otherwise
// "<dir>/<uptimePrefix>_<uptimeS>". No extension: see nextFreePath().
inline void timestampedBase(char* out, size_t n, const char* dir, const char* uptimePrefix,
                            uint16_t year, uint8_t month, uint8_t day,
                            uint8_t hour, uint8_t minute, uint8_t second,
                            unsigned long uptimeS) {
  if (year < 2020) {
    snprintf(out, n, "%s/%s_%lu", dir, uptimePrefix, uptimeS);
  } else {
    snprintf(out, n, "%s/%04u%02u%02u_%02u%02u%02u", dir, year, month, day, hour, minute, second);
  }
}

// First of "<base><ext>", "<base>_1<ext>", "<base>_2<ext>", ... that does not
// exist yet. FILE_WRITE truncates, so callers never open an existing path.
inline void nextFreePath(char* out, size_t n, const char* base, const char* ext) {
  snprintf(out, n, "%s%s", base, ext);
  for (unsigned long suffix = 1; SD_MMC.exists(out); ++suffix)
    snprintf(out, n, "%.48s_%lu%s", base, suffix, ext);
}

} // namespace storage

#endif // OPENCYCLO_STORAGE_UNIQUE_PATH_H
