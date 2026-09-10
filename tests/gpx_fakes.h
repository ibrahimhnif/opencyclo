#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <string>

inline unsigned long millis() { return 42000; }
struct GpxSerial {
  template<typename... Args> void printf(const char*, Args...) {}
};
extern GpxSerial Serial;
enum RideSaveState { RIDE_SAVE_NONE, RIDE_SAVE_PENDING, RIDE_SAVE_OK, RIDE_SAVE_ERROR, RIDE_SAVE_NO_FILE };
struct TelemetryState {
  double lat = 0, lon = 0; float altitude_m = 0;
  uint16_t gps_year=0;
  uint8_t gps_month=0,gps_day=0,gps_hour=0,gps_minute=0,gps_second=0;
};
extern size_t writeLimit;

class File {
  std::string* content_ = nullptr;
 public:
  File() = default;
  explicit File(std::string* content) : content_(content) {}
  explicit operator bool() const { return content_ != nullptr; }
  size_t print(const char* value) { size_t n=std::min(writeLimit,strlen(value));content_->append(value,n);return n; }
  size_t println(const char* value) { return print(value) + print("\n"); }
  template<typename... Args> void printf(const char* format, Args... args) {
    char buffer[512];
    snprintf(buffer, sizeof(buffer), format, args...);
    print(buffer);
  }
  void flush() {}
  void close() { content_ = nullptr; }
};
enum { FILE_WRITE = 1 };
struct FakeCard {
  std::map<std::string, std::string> files;
  bool failOpen = false;
  bool exists(const char* name) const { return files.count(name) != 0; }
  void mkdir(const char*) {}
  File open(const char* name, int) {
    if (failOpen) return File();
    files[name].clear(); // Match FILE_WRITE truncation, so collisions fail tests.
    return File(&files[name]);
  }
};
extern FakeCard SD_MMC;
