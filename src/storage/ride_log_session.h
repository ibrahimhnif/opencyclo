#pragma once
#include "gpx_writer.h"

// Called only by the logger with the SD lock held. UI never owns the file.
inline RideSaveState finishRideLog(GpxWriter& writer,bool mounted,bool enabled,const TelemetryState& s) {
  if(!writer.isOpen()) {
    if(!mounted || !enabled)return RIDE_SAVE_NO_FILE;
    writer.openNewRideFile(s.gps_year,s.gps_month,s.gps_day,s.gps_hour,s.gps_minute,s.gps_second);
    if(!writer.isOpen())return RIDE_SAVE_ERROR;
  }
  return writer.closeRideFile()?RIDE_SAVE_OK:RIDE_SAVE_ERROR;
}
