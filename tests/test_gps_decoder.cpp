#include "hardware/gps_decoder.h"
#include <cassert>
#include <cstdio>
#include <string>
unsigned long fakeTime=100;
static void feed(GpsDecoder& gps,const std::string& body,bool corrupt=false) {
  unsigned checksum=0;for(char c:body)checksum^=uint8_t(c);
  char suffix[8];snprintf(suffix,sizeof(suffix),"*%02X\r\n",checksum^(corrupt?1:0));
  for(char c:"$"+body+suffix)gps.feed(c,fakeTime);
}
static void rmc(GpsDecoder& gps,const char* speed,const char* status="A") {
  feed(gps,std::string("GNRMC,120000.00,")+status+",0612.0000,S,10648.0000,E,"+speed+",0.0,100926,,,A");
}
int main() {
  GpsDecoder gps;
  assert(!gps.snapshot(fakeTime).isValid);
  rmc(gps,"10.0");auto f=gps.snapshot(fakeTime);
  assert(f.isValid&&f.speedValid&&std::abs(f.speedKmh-18.52)<0.01);
  fakeTime+=1600;
  feed(gps,"GNGGA,120002.00,0612.0000,S,10648.0000,E,1,10,0.8,20.0,M,0,M,,");
  f=gps.snapshot(fakeTime);assert(f.isValid&&!f.speedValid&&f.speedKmh==0);
  rmc(gps,"");assert(!gps.snapshot(fakeTime).speedValid);
  rmc(gps,"75.0");assert(!gps.snapshot(fakeTime).speedValid); // 138.9 km/h rejected
  rmc(gps,"162.0");assert(!gps.snapshot(fakeTime).speedValid);
  rmc(gps,"nan");assert(!gps.snapshot(fakeTime).speedValid);
  rmc(gps,"-1");assert(!gps.snapshot(fakeTime).speedValid);
  rmc(gps,"12");assert(gps.snapshot(fakeTime).speedValid);
  rmc(gps,"12","V");assert(!gps.snapshot(fakeTime).isValid);
  gps.reset();rmc(gps,"");assert(!gps.snapshot(fakeTime).speedValid);
  feed(gps,"GNRMC,120000.00,A,0612.0000,S,10648.0000,E,10,0,100926,,,A",true);
  assert(!gps.snapshot(fakeTime).isValid);
  for(int i=0;i<100;i++){rmc(gps,"15");fakeTime+=200;}
  assert(gps.snapshot(fakeTime).speedValid);
  fakeTime+=1500;assert(!gps.snapshot(fakeTime).isValid);
  gps.reset();feed(gps,"GNGGA,120002.00,0612.0000,S,10648.0000,E,0,00,99.9,0,M,0,M,,");
  assert(!gps.snapshot(fakeTime).isValid);
  puts("GPS decoder regression tests passed (real TinyGPS++).");
}
