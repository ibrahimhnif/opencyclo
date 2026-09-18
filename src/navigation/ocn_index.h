#pragma once
#include <cstdint>
#include <cstddef>
namespace ocn {
// Pure binary search over a sorted-by-y OCN1 index table. `index` points to
// `indexCount` consecutive 12-byte little-endian entries (y, offset, count),
// exactly as they appear on disk starting at file offset 12. Returns true
// and fills *offsetOut/*countOut if row `y` is present; false otherwise.
// Never touches SD/files -- purely operates on an in-memory buffer, so the
// caller is responsible for having already read the index table into RAM.
inline bool findRow(const uint8_t* index, uint32_t indexCount, uint32_t y,
                    uint32_t* offsetOut, uint32_t* countOut) {
  uint32_t lo = 0, hi = indexCount;
  while (lo < hi) {
    uint32_t mid = (lo + hi) / 2;
    uint32_t entryY, entryOffset, entryCount;
    const uint8_t* e = index + mid * 12;
    // Little-endian u32 reads, portable regardless of host/target endianness.
    entryY = uint32_t(e[0]) | uint32_t(e[1]) << 8 | uint32_t(e[2]) << 16 | uint32_t(e[3]) << 24;
    entryOffset = uint32_t(e[4]) | uint32_t(e[5]) << 8 | uint32_t(e[6]) << 16 | uint32_t(e[7]) << 24;
    entryCount = uint32_t(e[8]) | uint32_t(e[9]) << 8 | uint32_t(e[10]) << 16 | uint32_t(e[11]) << 24;
    if (entryY < y) lo = mid + 1;
    else if (entryY > y) hi = mid;
    else { *offsetOut = entryOffset; *countOut = entryCount; return true; }
  }
  return false;
}
}
