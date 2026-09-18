#pragma once
#include "core/telemetry_state.h"
#include <cmath>
inline bool rideSpeedIsValid(const TelemetryState& state,const GpsFix& fix,uint32_t now) {
  if(!std::isfinite(state.speed_kmh)||state.speed_kmh<0)return false;
  if(state.speed_source==SPEED_SOURCE_BLE_CSC)return now-state.csc_sample_at_ms<=5000;
  return state.speed_source==SPEED_SOURCE_GPS && fix.isValid && fix.speedValid &&
    now-fix.receivedAtMs<1500 && fix.ageMs<1500;
}
