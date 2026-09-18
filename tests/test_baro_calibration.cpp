#include "../src/hardware/baro_calibration.h"
#include <cassert>
#include <initializer_list>
#include "../src/core/altitude_policy.h"
int main(){
  AltitudeCalibrationWindow window;
  int elevation=0;
  for(uint32_t t=100;t<30100;t+=200){
    assert(!window.update(t,true,5,elevation));
    assert(!window.update(t,true,5,elevation)); // duplicates do not accelerate
  }
  assert(window.update(30100,true,5,elevation) && elevation==5);
  assert(!window.update(31000,false,5,elevation));
  assert(!window.update(32000,true,5,elevation));
  assert(!window.update(34000,true,5,elevation)); // gap restarts window
  assert(!window.update(34200,true,20,elevation)); // unstable altitude
  assert(!window.update(34400,true,NAN,elevation));
  assert(!window.update(34600,true,9001,elevation));
  GpsAltitudeFilter filter;
  assert(filter.update(100,true,5)==5);
  float smooth=filter.update(300,true,15);
  assert(smooth>5 && smooth<6);
  assert(filter.update(300,true,15)==smooth);
  assert(filter.update(2000,true,20)==20); // stale gap resets
  assert(filter.update(2200,false,20)==0);
  assert(filter.update(2400,true,7)==7);
  assert(std::abs(baroReferenceForElevation(1013.25f,0)-1013.25f)<0.01f);
  for(int elevation : {-400,5,100,1000,5000,8000}){
    float pressure=1013.25f*std::pow(1.0f-elevation/44330.0f,1.0f/0.1903f);
    if(pressure>300 && pressure<1100){
      float ref=baroReferenceForElevation(pressure,elevation);
      assert(std::abs(ref-1013.25f)<0.1f);
    }
  }
  assert(!baroReferenceValid(NAN));
  assert(!baroReferenceValid(0));
  assert(std::isnan(baroReferenceForElevation(NAN,5)));
  assert(std::isnan(baroReferenceForElevation(1013,9001)));
  assert(std::isnan(baroReferenceForElevation(1013,-501)));
  assert(std::isnan(baroReferenceForElevation(0,5)));
}
