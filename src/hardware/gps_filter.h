#pragma once
#include "core/telemetry_state.h"
#include <algorithm>
#include <cmath>

// Application policy, not Garmin/iGPSPORT's proprietary algorithm. No heap.
// Qualify observations BEFORE map, auto-resume, statistics and GPX see them.
class GpsFilter {
  bool have=false,ready=false,moving=false,starting=false,stopping=false;
  uint32_t goodSince=0,startSince=0,stopSince=0;
  GpsFix last{},output{};
  double anchorLat=0,anchorLon=0;
  static double distance(double a,double b,double c,double d) {
    const double r=3.141592653589793/180;
    double x=std::sin((c-a)*r/2),y=std::sin((d-b)*r/2);
    double h=x*x+std::cos(a*r)*std::cos(c*r)*y*y;
    return 12742000*std::asin(std::sqrt(std::min(1.0,std::max(0.0,h))));
  }
  GpsFix reject(GpsFix f) {
    have=ready=moving=starting=stopping=false;
    f.quality=f.isValid?1:0;f.isValid=f.speedValid=false;f.speedKmh=0;
    output=f;return f;
  }
public:
  void reset(){*this=GpsFilter();}
  GpsFix apply(GpsFix f,uint32_t now) {
    const bool fresh=f.ageMs<1500 && now-f.receivedAtMs<1500;
    const bool accuracy=f.accuracyValid?
      (std::isfinite(f.horizontalAccuracyM)&&f.horizontalAccuracyM>0 && f.horizontalAccuracyM<=15 &&
       std::isfinite(f.speedAccuracyMps)&&f.speedAccuracyMps>=0 && f.speedAccuracyMps<=0.6f && f.satellites>=6):
      (std::isfinite(f.hdop)&&f.hdop>0 && f.hdop<=2 && f.satellites>=8);
    if(!fresh)f.isValid=false;
    if(!f.isValid || !f.speedValid || !accuracy ||
       !std::isfinite(f.speedKmh) || f.speedKmh<0 || f.speedKmh>120 ||
       !std::isfinite(f.latitude)||!std::isfinite(f.longitude)||
       std::abs(f.latitude)>90||std::abs(f.longitude)>180)return reject(f);
    // Heartbeats and interleaved NMEA must not count as new PVT observations.
    if(have && f.receivedAtMs==last.receivedAtMs)return output;
    const float error=f.accuracyValid?f.horizontalAccuracyM:5;
    if(have) {
      const uint32_t dt=f.receivedAtMs-last.receivedAtMs;
      if(dt>=1500)return reject(f);
      const double step=distance(last.latitude,last.longitude,f.latitude,f.longitude);
      const float previousError=last.accuracyValid?last.horizontalAccuracyM:5;
      const double limit=std::max(last.speedKmh,f.speedKmh)/3.6*(dt/1000.0)+2*(error+previousError);
      if(step>limit || std::abs(f.speedKmh-last.speedKmh)>8+25*(dt/1000.0))return reject(f);
    } else {
      have=true;goodSince=f.receivedAtMs;anchorLat=f.latitude;anchorLon=f.longitude;
    }
    last=f;
    if(!ready) {
      if(f.receivedAtMs-goodSince<2000) {
        f.quality=1;f.isValid=f.speedValid=false;f.speedKmh=0;output=f;return f;
      }
      ready=true;
    }
    // Two independent checks to exit hold: sustained Doppler speed above
    // its uncertainty, AND displacement outside the held position's radius.
    const float uncertainty=f.accuracyValid?2*f.speedAccuracyMps*3.6f:3.0f;
    const float startThreshold=std::max(1.5f,uncertainty);
    const float stopThreshold=std::max(1.0f,uncertainty*0.5f);
    if(!moving) {
      if(f.speedKmh>startThreshold) {
        if(!starting){starting=true;startSince=f.receivedAtMs;}
        if(f.receivedAtMs-startSince>=2000 &&
           distance(anchorLat,anchorLon,f.latitude,f.longitude)>std::max(3.0f,1.5f*error))moving=true;
      } else starting=false;
    } else if(f.speedKmh<stopThreshold) {
      if(!stopping){stopping=true;stopSince=f.receivedAtMs;}
      if(f.receivedAtMs-stopSince>=1500) {
        moving=starting=stopping=false;anchorLat=f.latitude;anchorLon=f.longitude;
      }
    } else stopping=false;
    if(!moving) {f.latitude=anchorLat;f.longitude=anchorLon;f.speedKmh=0;}
    f.quality=2;output=f;return f;
  }
};
