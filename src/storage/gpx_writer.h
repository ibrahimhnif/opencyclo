#ifndef OPENCYCLO_STORAGE_GPX_WRITER_H
#define OPENCYCLO_STORAGE_GPX_WRITER_H

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include "core/telemetry_state.h"

class GpxWriter {
  File _file;
  bool _isOpen;
  uint16_t _bufferCount;
  char _filename[64];

public:
  GpxWriter();
  bool openNewRideFile(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
  bool appendTrackPoint(const TelemetryState& state, uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
  bool closeRideFile();
  bool isOpen() const { return _isOpen; }
  const char* getFilename() const { return _filename; }
};

#endif // OPENCYCLO_STORAGE_GPX_WRITER_H
