#include "gpx_writer.h"
#include <stdio.h>

GpxWriter::GpxWriter() : _isOpen(false), _bufferCount(0), _pendingOffset(0), _closing(false) {
  _filename[0] = '\0';
  _pending[0] = '\0';
}
bool GpxWriter::flushPending() {
  size_t length=strlen(_pending);
  if(_pendingOffset<length) {
    size_t written=_file.print(_pending+_pendingOffset);
    _pendingOffset+=written;
    if(_pendingOffset<length)return false;
  }
  _pending[0]=0;_pendingOffset=0;return true;
}

bool GpxWriter::openNewRideFile(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
  if (_isOpen) {
    if(!closeRideFile())return false;
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

  // Preserve earlier rides even when the clock is unavailable or repeated.
  // FILE_WRITE truncates, so never open an existing path for a new ride.
  char baseName[64];
  snprintf(baseName, sizeof(baseName), "%s", _filename);
  baseName[strlen(baseName) - 4] = '\0'; // remove .gpx
  uint32_t suffix = 0;
  while (SD_MMC.exists(_filename)) {
    snprintf(_filename, sizeof(_filename), "%.48s_%lu.gpx", baseName, (unsigned long)++suffix);
  }
  _file = SD_MMC.open(_filename, FILE_WRITE);
  if (!_file) {
    Serial.printf("[GPX ERROR] Failed to create GPX file: %s\n", _filename);
    _isOpen = false;
    return false;
  }

  _isOpen = true;
  _bufferCount = 0;
  _lastTimestamp=0;
  _closing=false;_pendingOffset=0;

  // Write GPX XML Header
  strcpy(_pending,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<gpx version=\"1.1\" creator=\"OpenCyclo Cycling Computer\""
    " xmlns=\"http://www.topografix.com/GPX/1/1\">\n"
    "  <metadata><name>OpenCyclo Ride</name></metadata>\n"
    "  <trk><name>OpenCyclo Track</name>\n    <trkseg>\n");
  if(!flushPending())return false;
  _file.flush();

  Serial.printf("[GPX SUCCESS] Started new GPX log file: %s\n", _filename);
  return true;
}

bool GpxWriter::appendTrackPoint(const TelemetryState& state, uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
  if (!_isOpen || !_file || _closing || !flushPending()) return false;

  // UTC seconds, independent of local timezone. Reject duplicate/backward
  // epochs even when the logger tick and GNSS second boundaries drift.
  uint64_t stamp=0;
  if(year>=2020 && year<=2199 && month>=1 && month<=12 && day>=1 && day<=31 && hour<24 && minute<60 && second<60) {
    auto leap=[](unsigned y){return y%4==0 && (y%100!=0 || y%400==0);};
    const unsigned lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(day>lengths[month-1]+(month==2 && leap(year)))return false;
    uint64_t days=0;
    for(unsigned y=2020;y<year;++y)days+=leap(y)?366:365;
    for(unsigned m=1;m<month;++m)days+=lengths[m-1]+(m==2 && leap(year));
    stamp=((days+day-1)*24+hour)*3600+minute*60+second+1;
    if(_lastTimestamp && stamp<=_lastTimestamp)return true;
  } else if(_lastTimestamp)return false;
  const bool gap=_lastTimestamp && stamp>_lastTimestamp+5;

  char timeTag[96]={0};
  char elevationTag[48]={0};
  char extensionsTag[128]={0};
  if(state.altitude_valid)snprintf(elevationTag,sizeof(elevationTag),"        <ele>%.1f</ele>\n",state.altitude_m);
  if(year>=2020 && month>=1 && month<=12 && day>=1 && day<=31)
    snprintf(timeTag,sizeof(timeTag),"        <time>%04u-%02u-%02uT%02u:%02u:%02uZ</time>\n",
      year,month,day,hour,minute,second);
  if(state.heart_rate_bpm>=0 || state.cadence_rpm>=0) {
    char hrTag[32]={0},cadTag[32]={0};
    if(state.heart_rate_bpm>=0)snprintf(hrTag,sizeof(hrTag),"<gpxtpx:hr>%d</gpxtpx:hr>",state.heart_rate_bpm);
    if(state.cadence_rpm>=0)snprintf(cadTag,sizeof(cadTag),"<gpxtpx:cad>%d</gpxtpx:cad>",state.cadence_rpm);
    snprintf(extensionsTag,sizeof(extensionsTag),
             "        <extensions><gpxtpx:TrackPointExtension>%s%s</gpxtpx:TrackPointExtension></extensions>\n",
             hrTag,cadTag);
  }
  snprintf(_pending, sizeof(_pending),
           "%s      <trkpt lat=\"%.6f\" lon=\"%.6f\">\n"
           "%s"
           "%s"
           "%s"
           "      </trkpt>\n",
           gap?"    </trkseg>\n    <trkseg>\n":"",state.lat, state.lon, elevationTag,
           timeTag, extensionsTag);

  // Pending bytes belong to this epoch, including a short-write retry.
  _lastTimestamp=stamp;

  if(!flushPending())return false;
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

  // Resume short writes at the exact byte; retry must never duplicate XML.
  if(!flushPending())return false;
  if(!_closing) {
    _closing=true;
    strcpy(_pending,"    </trkseg>\n  </trk>\n</gpx>\n");
  }
  if(!flushPending())return false;
  _file.flush();
  _file.close();

  _isOpen = false;
  Serial.printf("[GPX SUCCESS] Closed GPX log file: %s\n", _filename);
  return true;
}
