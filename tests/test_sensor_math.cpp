#include "core/display_speed.h"
#include "hardware/csc_decoder.h"
#include "storage/ride_export.h"
#include <cassert>
#include <cstdio>
#include <vector>
std::vector<uint8_t> packet(uint32_t wheel,uint16_t wt,uint16_t crank,uint16_t ct) {
  return {3,uint8_t(wheel),uint8_t(wheel>>8),uint8_t(wheel>>16),uint8_t(wheel>>24),uint8_t(wt),uint8_t(wt>>8),uint8_t(crank),uint8_t(crank>>8),uint8_t(ct),uint8_t(ct>>8)};
}
int main() {
  DisplaySpeed display;
  assert(display.update(18,1,0)==18);
  for(uint32_t t=50;t<=3000;t+=50)display.update((t/50)%2?20:18,1,t);
  assert(std::abs(display.update(18,1,3050)-19)<0.1);
  assert(display.update(0,1,3100)==0);
  assert(display.update(25,2,3200)==25);
  assert(display.update(25,0,3300)==0);
  assert(display.update(NAN,1,3400)==0);
  CscDecoder d;auto p=packet(UINT32_MAX,65000,65535,65000);
  assert(d.update(p.data(),p.size(),100,2096));
  assert(d.speedKmh(100)<0 && d.cadenceRpm(100)<0);
  p=packet(0,488,0,488);assert(d.update(p.data(),p.size(),1100,2096));
  assert(std::abs(d.speedKmh(1100)-7.5456)<0.001 && d.cadenceRpm(1100)==60);
  assert(!d.update(p.data(),p.size()-1,2000,2096));
  d.update(p.data(),p.size(),4200,2096);
  assert(d.speedKmh(4200)==0 && d.cadenceRpm(4200)==0);
  assert(d.speedKmh(9300)<0 && d.cadenceRpm(9300)<0);
  assert(safeRideName("20260911_123000.gpx"));
  assert(!safeRideName("../secret.gpx") && !safeRideName("x/y.gpx") && !safeRideName("file.gpx\n"));
  puts("sensor math tests passed");
}
