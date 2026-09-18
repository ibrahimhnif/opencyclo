#pragma once
#include "core/telemetry_state.h"
#ifndef GPS_DIAGNOSTICS_ENABLED
#define GPS_DIAGNOSTICS_ENABLED 0
#endif
#if GPS_DIAGNOSTICS_ENABLED
void captureGpsDiagnostic(const GpsFix& raw,const GpsFix& filtered,const char* reason,uint32_t now,const char* nmea="");
// Logger task only, with SdGuard held. Never perform SD writes in GPS task.
void drainGpsDiagnostics(const TelemetryState& state,const char* rideFile);
void closeGpsDiagnostics();
#else
inline void captureGpsDiagnostic(const GpsFix&,const GpsFix&,const char*,uint32_t,const char* = "") {}
inline void drainGpsDiagnostics(const TelemetryState&,const char*) {}
inline void closeGpsDiagnostics() {}
#endif
