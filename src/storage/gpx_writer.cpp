#include "gpx_writer.h"
#include <stdio.h>

GpxWriter::GpxWriter() : _isOpen(false), _bufferCount(0) {
  _filename[0] = '\0';
}

bool GpxWriter::openNewRideFile(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
  if (_isOpen) {
    closeRideFile();
  }

  if (!SD_MMC.exists("/rides")) {
    SD_MMC.mkdir("/rides");
  }

  if (year < 2020) {
    snprintf(_filename, sizeof(_filename), "/rides/ride_%lu.gpx", millis() / 1000);
  } else {
    snprintf(_filename, sizeof(_filename), "/rides/%04u%02u%02u_%02u%02u%02u.gpx",
             year, month, day, hour, minute, second);
  }

  _file = SD_MMC.open(_filename, FILE_WRITE);
  if (!_file) {
    Serial.printf("[GPX ERROR] Failed to create GPX file: %s\n", _filename);
    _isOpen = false;
    return false;
  }

  _isOpen = true;
  _bufferCount = 0;

  // Write GPX XML Header
  _file.println("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  _file.println("<gpx version=\"1.1\" creator=\"OpenCyclo Cycling Computer\"");
  _file.println("  xmlns=\"http://www.topografix.com/GPX/1/1\"");
  _file.println("  xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"");
  _file.println("  xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd\">");
  _file.println("  <metadata>");
  _file.printf("    <name>OpenCyclo Ride %04u-%02u-%02u</name>\n", year, month, day);
  _file.println("  </metadata>");
  _file.println("  <trk>");
  _file.println("    <name>OpenCyclo Track</name>");
  _file.println("    <trkseg>");
  _file.flush();

  Serial.printf("[GPX SUCCESS] Started new GPX log file: %s\n", _filename);
  return true;
}

bool GpxWriter::appendTrackPoint(const TelemetryState& state, uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
  if (!_isOpen || !_file) return false;

  char buf[256];
  snprintf(buf, sizeof(buf),
           "      <trkpt lat=\"%.6f\" lon=\"%.6f\">\n"
           "        <ele>%.1f</ele>\n"
           "        <time>%04u-%02u-%02uT%02u:%02u:%02uZ</time>\n"
           "      </trkpt>\n",
           state.lat, state.lon, state.altitude_m,
           year, month, day, hour, minute, second);

  _file.print(buf);
  _bufferCount++;

  // Flush buffer every 10 points (~10 seconds)
  if (_bufferCount >= 10) {
    _file.flush();
    _bufferCount = 0;
  }

  return true;
}

bool GpxWriter::closeRideFile() {
  if (!_isOpen || !_file) return false;

  _file.println("    </trkseg>");
  _file.println("  </trk>");
  _file.println("</gpx>");
  _file.flush();
  _file.close();

  _isOpen = false;
  Serial.printf("[GPX SUCCESS] Closed GPX log file: %s\n", _filename);
  return true;
}
