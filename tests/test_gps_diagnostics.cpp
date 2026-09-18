#include "diagnostic_fakes.h"
#include "storage/gps_diagnostics.cpp"
#include <cassert>
unsigned long fakeTime=100;
size_t writeLimit=SIZE_MAX;
DebugSerial Serial;
DebugCard SD_MMC;
int main() {
  GpsDebugSampling policy;
  assert(policy.decision(100,"accepted",4,3,true));
  assert(!policy.decision(110,"accepted",4,3,true));
  assert(policy.decision(120,"satellites",3,3,false)); // immediate transition
  assert(policy.decision(320,"satellites",3,3,false));
  assert(policy.nmea(100,"$GPGSV,2,1,08,01,12,229,16,1*00"));
  assert(!policy.nmea(110,"$GPGSV,2,1,08,01,12,229,35,1*00"));
  assert(policy.nmea(110,"$GPGSV,2,2,08,02,12,229,35,1*00"));
  assert(policy.nmea(110,"$GPGSV,2,1,08,01,12,229,35,7*00"));
  assert(policy.nmea(1100,"$GPGSV,2,1,08,01,12,229,35,1*00"));
  GpsDebugSampling wrap;
  assert(wrap.nmea(UINT32_MAX-100,"$GNGGA,0*00"));
  assert(!wrap.nmea(50,"$GNGGA,0*00"));
  assert(wrap.nmea(1000,"$GNGGA,0*00"));
  // High-rate observations reduce to 5 Hz decisions + 1 Hz per signal page.
  GpsDebugSampling burst;unsigned retained=0;
  for(uint32_t t=0;t<10000;t+=10) {
    retained+=burst.decision(t,"no-fix",0,0,false);
    retained+=burst.nmea(t,"$GPGSV,2,1,08,01,12,229,35,1*00");
    retained+=burst.nmea(t,"$GPGSV,2,2,08,02,12,229,35,1*00");
  }
  assert(retained==70 && burst.sampledOut==2930);
  GpsFix r{},f{};TelemetryState state{};
  r.year=2026;r.month=9;r.day=13;r.satellites=4;r.latitude=-6.2;
  r.receivedAtMs=50;
  for(int i=0;i<70;++i)captureGpsDiagnostic(r,f,"satellites",100+i*200);
#if GPS_DIAGNOSTICS_ENABLED
  assert(count==64 && dropped==6);
  for(int i=0;i<4;++i)drainGpsDiagnostics(state,"/rides/test.gpx");
  assert(count==0);
  auto saved=SD_MMC.files["/debug/gps_boot_00001.csv"];
  assert(saved.find("satellites,6,0,/rides/test.gpx")!=std::string::npos);
  assert(std::count(saved.begin(),saved.end(),'\n')==66);
  captureGpsDiagnostic(r,f,"nmea-observation",190,"$GPGSV,1,1,01,03,45,123,32,1*00");
  drainGpsDiagnostics(state,"");
  saved=SD_MMC.files["/debug/gps_boot_00001.csv"];
  assert(saved.find("\"$GPGSV,1,1,01,03,45,123,32,1*00\"")!=std::string::npos);
  closeGpsDiagnostics();
  captureGpsDiagnostic(r,f,"no-fix",200);
  drainGpsDiagnostics(state,"");
  assert(SD_MMC.files.count("/debug/gps_boot_00002.csv"));
  assert(SD_MMC.files["/debug/gps_boot_00001.csv"]==saved);
  writeLimit=2;captureGpsDiagnostic(r,f,"stale",300);
  drainGpsDiagnostics(state,"");assert(disabled && !file);
  // Size cap must stop writes, without overwriting or deleting earlier files.
  disabled=false;writeLimit=SIZE_MAX;bytes=maxBytes;
  drainGpsDiagnostics(state,"");assert(disabled && !file);
#else
  drainGpsDiagnostics(state,"");closeGpsDiagnostics();assert(SD_MMC.files.empty());
#endif
  puts("GPS diagnostics overflow, CSV linkage, collision, failure/cap and disabled mode passed");
}
