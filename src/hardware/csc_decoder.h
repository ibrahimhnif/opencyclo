#pragma once
#include <stdint.h>
#include <stddef.h>
// Bluetooth SIG CSC Measurement, little endian, event clock 1/1024 second.
class CscDecoder {
  uint32_t wheel=0,lastWheel=0,lastCrank=0,lastPacket=0;
  uint16_t wheelTime=0,crank=0,crankTime=0;
  bool wheelBase=false,crankBase=false;
  float speed=-1,cadence=-1;
  static uint16_t u16(const uint8_t* p){return p[0]|uint16_t(p[1])<<8;}
public:
  bool wheelSeen=false,crankSeen=false;
  bool update(const uint8_t* p,size_t n,uint32_t now,uint16_t circumference) {
    if(!n || (p[0]&~3) || !(p[0]&3) || n!=size_t(1+((p[0]&1)?6:0)+((p[0]&2)?4:0)))return false;
    if(now-lastPacket>5000){wheelBase=false;crankBase=false;speed=cadence=-1;}
    lastPacket=now;size_t offset=1;
    if(p[0]&1) {
      uint32_t rev=uint32_t(p[1])|uint32_t(p[2])<<8|uint32_t(p[3])<<16|uint32_t(p[4])<<24;
      uint16_t time=u16(p+5),dt=uint16_t(time-wheelTime);uint32_t dr=rev-wheel;
      wheelSeen=true;
      if(wheelBase && dr && dt) {
        float value=double(dr)*circumference*3.6*1.024/dt;
        speed=value<=120?value:-1;lastWheel=now;
      } else if(!wheelBase){lastWheel=now;speed=-1;}
      wheel=rev;wheelTime=time;wheelBase=true;offset=7;
    }
    if(p[0]&2) {
      uint16_t rev=u16(p+offset),time=u16(p+offset+2),dt=uint16_t(time-crankTime),dr=uint16_t(rev-crank);
      crankSeen=true;
      if(crankBase && dr && dt) {
        float value=float(dr)*60*1024/dt;
        cadence=value<=250?value:-1;lastCrank=now;
      } else if(!crankBase){lastCrank=now;cadence=-1;}
      crank=rev;crankTime=time;crankBase=true;
    }
    return true;
  }
  float speedKmh(uint32_t now) const {return !wheelSeen||now-lastPacket>5000?-1:now-lastWheel>3000?0:speed;}
  int cadenceRpm(uint32_t now) const {return !crankSeen||now-lastPacket>5000?-1:now-lastCrank>3000?0:int(cadence);}
  uint32_t age(uint32_t now) const{return (wheelSeen||crankSeen)?now-lastPacket:UINT32_MAX;}
};
