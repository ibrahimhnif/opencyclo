#pragma once
#include <cmath>
#include <stdint.h>

// One fresh receiver epoch per observation. Never treat repeated queue snapshots
// as new evidence. Calibration is deliberately stricter than map position.
class AltitudeCalibrationWindow {
  uint32_t start=0,last=0,count=0;
  float sum=0,low=0,high=0;
public:
  void reset(){count=0;}
  bool update(uint32_t epoch, bool eligible, float altitude, int& result) {
    if(!eligible || !std::isfinite(altitude) || altitude < -500 || altitude > 9000){reset();return false;}
    if(count && epoch==last)return false;
    if(count && epoch-last>=1500)reset();
    if(!count){start=epoch;low=high=altitude;sum=0;}
    if(altitude<low)low=altitude;
    if(altitude>high)high=altitude;
    if(high-low>6){reset();return false;}
    last=epoch;sum+=altitude;++count;
    if(epoch-start<30000 || count<25)return false;
    result=static_cast<int>(std::lround(sum/count));reset();return true;
  }
};

class GpsAltitudeFilter {
  bool valid=false;
  uint32_t last=0;
  float value=0;
public:
  float update(uint32_t epoch,bool usable,float altitude){
    if(!usable){valid=false;return 0;}
    if(!valid || epoch-last>=1500){value=altitude;valid=true;}
    else if(epoch!=last){
      float alpha=float(epoch-last)/(3000.0f+float(epoch-last));
      value+=alpha*(altitude-value);
    }
    last=epoch;return value;
  }
};
