#include "gps_diagnostics.h"
#include "gps_debug_sampling.h"
#if GPS_DIAGNOSTICS_ENABLED
#include <SD_MMC.h>
#include <cstdio>
#include <cstring>

namespace {
struct Sample { GpsFix raw,filtered; uint32_t at; char reason[32]; char nmea[160]; };
Sample pending[64];
portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
unsigned head=0,count=0;
uint32_t dropped=0;
GpsDebugSampling sampling;
uint32_t sampledOut=0;
File file;
bool disabled=false;
uint32_t bytes=0,lastFlush=0;
constexpr uint32_t maxBytes=32u*1024*1024;
bool writeBytes(const char* text) {
  const size_t length=strlen(text);
  if(bytes+length>maxBytes || file.write(reinterpret_cast<const uint8_t*>(text),length)!=length) {
    file.close();disabled=true;
    Serial.println("[GPS DEBUG] stopped: write failed or 32 MiB boot limit reached");
    return false;
  }
  bytes+=length;return true;
}
}
void captureGpsDiagnostic(const GpsFix& raw,const GpsFix& filtered,const char* reason,uint32_t now,const char* nmea) {
  const bool keep=nmea[0]?sampling.nmea(now,nmea):
    sampling.decision(now,reason,raw.satellites,raw.receiverFixType,filtered.isValid);
  portENTER_CRITICAL(&mux);
  sampledOut=sampling.sampledOut;
  portEXIT_CRITICAL(&mux);
  if(!keep)return;
  Sample sample{raw,filtered,now,{0},{0}};
  snprintf(sample.reason,sizeof(sample.reason),"%s",reason);
  snprintf(sample.nmea,sizeof(sample.nmea),"%s",nmea);
  // Keep diagnostic NMEA in one safely quoted CSV field, even malformed text.
  for(char* p=sample.nmea;*p;++p)if(*p=='"'||*p=='\r'||*p=='\n')*p='?';
  portENTER_CRITICAL(&mux);
  // Reserve sixteen slots for filter decisions when signal messages burst.
  if(count==64 || (sample.nmea[0] && count>=48))++dropped;
  else {pending[(head+count)%64]=sample;++count;}
  portEXIT_CRITICAL(&mux);
}
void drainGpsDiagnostics(const TelemetryState& state,const char* rideFile) {
  if(disabled)return;
  if(!file) {
    if(!SD_MMC.exists("/debug") && !SD_MMC.mkdir("/debug")){disabled=true;Serial.println("[GPS DEBUG] directory failed");return;}
    char path[64];
    unsigned index=1;
    for(;index<=10000;++index) {
      snprintf(path,sizeof(path),"/debug/gps_boot_%05u.csv",index);
      if(!SD_MMC.exists(path))break;
    }
    if(index>10000){disabled=true;return;}
    file=SD_MMC.open(path,FILE_WRITE);
    if(!file){disabled=true;Serial.println("[GPS DEBUG] open failed");return;}
    Serial.printf("[GPS DEBUG] recording %s (schema 3, max 32 MiB)\n",path);
    if(!writeBytes("# schema=3; PVT minimum=4 satellites; NMEA minimum=5; diagnostic sampling enabled\n"
      "uptime_ms,received_ms,utc,raw_fix,speed_valid,accuracy_valid,satellites,hdop,hacc_m,sacc_mps,age_ms,raw_lat,raw_lon,raw_speed_kmh,accepted,filtered_lat,filtered_lon,filtered_speed_kmh,reason,dropped_total,logger_ride_state,logger_gpx,baseline_mode,pvt_fix_type,pvt_flags,pvt_flags3,nmea,sampled_out_total\n"))return;
  }
  // Bounded work per logger tick; overflow is counted, never blocks GPS.
  for(unsigned i=0;i<16;++i) {
    // Single logger consumer: keep formatting scratch off the task stack.
    static Sample sample;uint32_t lost,skipped;
    portENTER_CRITICAL(&mux);
    if(!count){portEXIT_CRITICAL(&mux);break;}
    sample=pending[head];head=(head+1)%64;--count;lost=dropped;skipped=sampledOut;
    portEXIT_CRITICAL(&mux);
    const auto& r=sample.raw;const auto& f=sample.filtered;
    static char row[896];
    int n=snprintf(row,sizeof(row),"%lu,%lu,%04u-%02u-%02uT%02u:%02u:%02uZ,%d,%d,%d,%lu,%.2f,%.2f,%.3f,%lu,%.7f,%.7f,%.3f,%d,%.7f,%.7f,%.3f,%s,%lu,%u,%s,%d,%u,%u,%u,\"%s\",%lu\n",
      (unsigned long)sample.at,(unsigned long)r.receivedAtMs,r.year,r.month,r.day,r.hour,r.minute,r.second,
      r.isValid,r.speedValid,r.accuracyValid,(unsigned long)r.satellites,r.hdop,r.horizontalAccuracyM,r.speedAccuracyMps,
      (unsigned long)r.ageMs,r.latitude,r.longitude,r.speedKmh,sample.nmea[0]?-1:int(f.isValid),f.latitude,f.longitude,f.speedKmh,
      // Keep the historical CSV column for existing analysis tools. Baseline
      // bypass was removed; all new records use the normal receiver lifecycle.
      sample.reason,(unsigned long)lost,unsigned(state.ride_state),rideFile,0,
      unsigned(r.receiverFixType),unsigned(r.receiverFlags),unsigned(r.receiverFlags3),sample.nmea,(unsigned long)skipped);
    if(n<0 || size_t(n)>=sizeof(row)){disabled=true;file.close();return;}
    if(!writeBytes(row))return;
  }
  if(millis()-lastFlush>=10000){file.flush();lastFlush=millis();}
}
void closeGpsDiagnostics() {
  // Power-off drains only the bounded queue before this call, then closes.
  if(file){file.flush();file.close();}
}
#endif
