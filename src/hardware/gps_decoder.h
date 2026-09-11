#pragma once
#include <TinyGPS++.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>
#include "core/telemetry_state.h"
#include "gnss_power.h"

// Admit whole checksum-verified sentences before TinyGPS++ can commit fields.
// TinyGPS++ retains old values on empty fields and isValid() is not freshness.
class GpsDecoder {
  TinyGPSPlus gps;
  char line[160]{};
  size_t used=0;
  bool collecting=false,positionOk=false,speedOk=false;
  uint32_t speedAt=0;
  gnss::Parser ubx;
  GpsFix pvt{};
  bool requirePvt=false,seenPvt=false;
  uint32_t epoch=0;
  static uint32_t u32(const uint8_t* p) {
    return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
  }
  static int32_t i32(const uint8_t* p) {
    uint32_t bits=u32(p);int32_t v;memcpy(&v,&bits,4);return v;
  }
  bool readPvt(uint32_t now) {
    const auto& frame=ubx.frame;
    if(frame.cls!=1 || frame.id!=7 || frame.length!=92)return false;
    const uint8_t* p=frame.payload;const uint32_t next=u32(p);
    // Duplicate/backwards epochs must not refresh a stopped receiver. Allow
    // the GNSS week rollover; recovery from a receiver reset expires in 1.5s.
    if(seenPvt && now-pvt.receivedAtMs<1500 &&
       (next==epoch || (next<epoch && epoch-next<604799000u)))return false;
    epoch=next;seenPvt=true;pvt=GpsFix{};pvt.receivedAtMs=now;
    pvt.latitude=i32(p+28)*1e-7;pvt.longitude=i32(p+24)*1e-7;
    pvt.altitudeM=i32(p+36)*0.001f;
    pvt.horizontalAccuracyM=u32(p+40)*0.001f;
    pvt.speedAccuracyMps=u32(p+68)*0.001f;pvt.accuracyValid=true;
    pvt.satellites=p[23];pvt.hdop=99.99f; // PVT pDOP is NOT HDOP.
    pvt.isValid=p[20]==3 && (p[21]&1) && !(p[78]&1) &&
      std::abs(pvt.latitude)<=90 && std::abs(pvt.longitude)<=180;
    pvt.speedKmh=i32(p+60)*0.0036f;
    pvt.speedValid=pvt.isValid && pvt.speedKmh>=0 && pvt.speedKmh<=120;
    if((p[11]&3)==3 && p[6]>=1 && p[6]<=12 && p[7]>=1 && p[7]<=31 &&
       p[8]<24 && p[9]<60 && p[10]<=60) {
      pvt.year=p[4]|uint16_t(p[5])<<8;pvt.month=p[6];pvt.day=p[7];
      pvt.hour=p[8];pvt.minute=p[9];pvt.second=p[10];
    }
    return true;
  }
  static int hex(char c) {
    if(c>='0'&&c<='9')return c-'0';
    if(c>='A'&&c<='F')return c-'A'+10;
    if(c>='a'&&c<='f')return c-'a'+10;
    return -1;
  }
  static bool number(const char* s) {
    if(!s||!*s)return false;
    bool digit=false,dot=false;
    if(*s=='-')s++;
    for(;*s;s++) {
      if(*s>='0'&&*s<='9')digit=true;
      else if(*s=='.'&&!dot)dot=true;
      else return false;
    }
    return digit;
  }
  bool sentence(uint32_t now) {
    char* star=strchr(line,'*');
    if(!star || strlen(star)!=3 || hex(star[1])<0 || hex(star[2])<0)return false;
    unsigned checksum=0;for(char* p=line+1;p<star;p++)checksum^=uint8_t(*p);
    if(checksum!=unsigned(hex(star[1])*16+hex(star[2])))return false;
    char copy[160];memcpy(copy,line,used+1);
    copy[star-line]=0;
    char* fields[24]{};size_t count=1;fields[0]=copy+1;
    for(char* p=copy+1;*p;p++)if(*p==',') {
      *p=0;if(count<24)fields[count++]=p+1;
    }
    if(strlen(fields[0])!=5 || fields[0][0]!='G' || !strchr("PNABL",fields[0][1]))return false;
    const bool rmc=strlen(fields[0])==5 && !strcmp(fields[0]+2,"RMC");
    const bool gga=strlen(fields[0])==5 && !strcmp(fields[0]+2,"GGA");
    if(!rmc&&!gga)return false;
    if((rmc&&count<10)||(gga&&count<10))return false;
    const int lat=rmc?3:2,lon=rmc?5:4;
    const bool coordinates=number(fields[lat])&&number(fields[lon]) &&
      (!strcmp(fields[lat+1],"N")||!strcmp(fields[lat+1],"S")) &&
      (!strcmp(fields[lon+1],"E")||!strcmp(fields[lon+1],"W"));
    positionOk=coordinates && number(fields[1]) &&
      (rmc?!strcmp(fields[2],"A"):(number(fields[6])&&atoi(fields[6])>0));
    if(!positionOk){speedOk=false;return true;}
    if(rmc) {
      const double knots=number(fields[7])?strtod(fields[7],nullptr):-1;
      // Cycling sanity ceiling: reject, never clamp an outlier into max speed.
      speedOk=knots>=0 && knots*1.852<=120 && std::isfinite(knots) && number(fields[9]);
      if(!speedOk)return true; // Do not commit an empty/invalid speed field.
      speedAt=now;
    } else if(!number(fields[7])||!number(fields[8])||!number(fields[9])) {
      positionOk=false;speedOk=false;return true;
    }
    for(size_t i=0;i<used;i++)gps.encode(line[i]);
    gps.encode('\r');gps.encode('\n');
    return true;
  }
public:
  uint32_t accepted=0,rejected=0;
  void reset(bool expectPvt=false) {
    // End lifetime before clearing storage; constructor leaves pending scalar
    // fields uninitialized. No copying from a temporary with indeterminate data.
    gps.~TinyGPSPlus();memset(static_cast<void*>(&gps),0,sizeof(gps));
    new(&gps)TinyGPSPlus();
    used=0;collecting=positionOk=speedOk=false;speedAt=0;accepted=rejected=0;
    ubx=gnss::Parser();pvt=GpsFix{};requirePvt=expectPvt;seenPvt=false;epoch=0;
  }
  GpsDecoder(){reset();}
  bool feed(char c,uint32_t now) {
    if(ubx.feed(uint8_t(c)) && readPvt(now))return true;
    if(c=='$'){used=0;collecting=true;line[used++]=c;return false;}
    if(!collecting)return false;
    if(c=='\r'||c=='\n') {
      collecting=false;line[used]=0;
      bool ok=sentence(now);if(ok)accepted++;else rejected++;
      return ok;
    }
    if(used>=sizeof(line)-1){collecting=false;rejected++;return false;}
    line[used++]=c;return false;
  }
  GpsFix snapshot(uint32_t now) {
    if(requirePvt || seenPvt) {
      GpsFix f=pvt;f.ageMs=seenPvt?now-pvt.receivedAtMs:UINT32_MAX;
      f.isValid=f.isValid && f.ageMs<1500;f.speedValid=f.speedValid&&f.isValid;
      if(!f.speedValid)f.speedKmh=0;
      if(gps.hdop.isValid()&&gps.hdop.age()<1500)f.hdop=gps.hdop.hdop();
      return f; // Never silently fall back to NMEA when PVT disappears.
    }
    GpsFix f{};
    f.ageMs=gps.location.age();f.receivedAtMs=now-f.ageMs;
    f.isValid=positionOk && gps.location.isValid() && f.ageMs<1500;
    f.latitude=gps.location.isValid()?gps.location.lat():0;
    f.longitude=gps.location.isValid()?gps.location.lng():0;
    f.isValid=f.isValid && std::isfinite(f.latitude)&&std::isfinite(f.longitude)&&
      std::abs(f.latitude)<=90&&std::abs(f.longitude)<=180;
    f.speedValid=f.isValid && speedOk && now-speedAt<1500 && gps.speed.isValid()&&gps.speed.age()<1500;
    f.speedKmh=f.speedValid?float(gps.speed.kmph()):0;
    f.speedValid=f.speedValid&&std::isfinite(f.speedKmh)&&f.speedKmh>=0;
    f.altitudeM=gps.altitude.isValid()?gps.altitude.meters():0;
    f.hdop=gps.hdop.isValid()&&gps.hdop.age()<1500?gps.hdop.hdop():99.99f;
    f.satellites=gps.satellites.isValid()&&gps.satellites.age()<1500?gps.satellites.value():0;
    if(gps.date.isValid()&&gps.time.isValid()&&gps.time.age()<1500) {
      f.year=gps.date.year();f.month=gps.date.month();f.day=gps.date.day();
      f.hour=gps.time.hour();f.minute=gps.time.minute();f.second=gps.time.second();
    }
    return f;
  }
};
