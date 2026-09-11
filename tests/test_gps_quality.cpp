#include "hardware/gps_decoder.h"
#include "hardware/gps_filter.h"
#include <cassert>
#include <cstdio>
#include <limits>
unsigned long fakeTime=100;
static void put(uint8_t* p,uint32_t x) {for(int i=0;i<4;i++)p[i]=uint8_t(x>>(8*i));}
static void frame(GpsDecoder& gps,uint32_t epoch,uint8_t type=3,uint8_t flags=1,
                  bool corrupt=false,bool invalidLlh=false,size_t length=92) {
  uint8_t p[92]{},bytes[100];put(p,epoch);p[4]=0xea;p[5]=7;p[6]=9;p[7]=11;p[11]=3;
  p[20]=type;p[21]=flags;p[23]=10;
  put(p+24,1068000000);put(p+28,uint32_t(-62000000));
  put(p+36,20000);put(p+40,3000);put(p+60,2000);put(p+68,200);p[78]=invalidLlh;
  size_t n=gnss::packet(1,7,p,length,bytes);if(corrupt)bytes[n-1]^=1;
  for(size_t i=0;i<n;i++)gps.feed(char(bytes[i]),fakeTime);
}
static GpsFix sample(uint32_t t,float speed=0,double metres=0) {
  GpsFix f{};f.isValid=f.speedValid=f.accuracyValid=true;
  f.receivedAtMs=t;f.latitude=-6.2+metres/111195.0;f.longitude=106.8;
  f.horizontalAccuracyM=3;f.speedAccuracyMps=0.2;f.satellites=10;f.hdop=0.9;
  f.speedKmh=speed;return f;
}
int main() {
  GpsDecoder decoder;decoder.reset(true);
  assert(!decoder.snapshot(fakeTime).isValid);
  frame(decoder,100);auto p=decoder.snapshot(fakeTime);
  assert(p.isValid && p.speedValid && p.accuracyValid);
  assert(std::abs(p.latitude+6.2)<1e-7 && std::abs(p.longitude-106.8)<1e-7);
  assert(std::abs(p.speedKmh-7.2)<0.001 && std::abs(p.horizontalAccuracyM-3)<0.001);
  assert(p.year==2026 && p.month==9 && p.day==11 && p.hdop>90);
  fakeTime=300;frame(decoder,100);assert(decoder.snapshot(fakeTime).receivedAtMs==100);
  frame(decoder,50);assert(decoder.snapshot(fakeTime).receivedAtMs==100);
  frame(decoder,300,3,1,true);assert(decoder.snapshot(fakeTime).receivedAtMs==100);
  frame(decoder,300,3,1,false,false,91);assert(decoder.snapshot(fakeTime).receivedAtMs==100);
  fakeTime=1700;assert(!decoder.snapshot(fakeTime).isValid);
  const char* nmea="$GNGGA,120002.00,0612.0000,S,10648.0000,E,1,10,0.8,20.0,M,0,M,,";
  uint8_t sum=0;for(const char* c=nmea+1;*c;c++)sum^=uint8_t(*c);
  char tail[8];snprintf(tail,sizeof(tail),"*%02X\r\n",sum);
  for(const char* c=nmea;*c;c++)decoder.feed(*c,fakeTime);
  for(const char* c=tail;*c;c++)decoder.feed(*c,fakeTime);
  assert(!decoder.snapshot(fakeTime).isValid); // Fresh GGA cannot revive missing PVT.
  frame(decoder,1700,2);assert(!decoder.snapshot(fakeTime).isValid);
  fakeTime+=200;frame(decoder,1900,3,0);assert(!decoder.snapshot(fakeTime).isValid);
  fakeTime+=200;frame(decoder,2100,3,1,false,true);assert(!decoder.snapshot(fakeTime).isValid);
  decoder.reset(true);fakeTime=2200;frame(decoder,604799800);
  fakeTime+=200;frame(decoder,0);assert(decoder.snapshot(fakeTime).receivedAtMs==2400);

  GpsFilter filter;GpsFix f{};
  // Actual indoor observation envelope: 5-8 satellites, HDOP 2.58-3.79,
  // false speed 2.8-6.5. Conservative NMEA fallback never calls this usable.
  for(uint32_t t=100;t<10000;t+=200) {
    auto raw=sample(t,6.5,t/1000.0);raw.accuracyValid=false;raw.hdop=3.34;raw.satellites=6;
    f=filter.apply(raw,t);assert(!f.isValid&&!f.speedValid&&f.speedKmh==0&&f.quality==1);
  }
  auto raw=sample(10000,6);raw.horizontalAccuracyM=40;
  assert(!filter.apply(raw,10000).isValid);
  raw=sample(10200,6);raw.speedAccuracyMps=2;
  assert(!filter.apply(raw,10200).isValid);
  // Two seconds of distinct good epochs required. Re-reading one fix cannot warm up.
  filter.reset();f=filter.apply(sample(100),100);assert(!f.isValid);
  for(int i=0;i<100;i++)assert(!filter.apply(sample(100),100).isValid);
  for(uint32_t t=300;t<=2100;t+=200)f=filter.apply(sample(t),t);
  assert(f.isValid&&f.speedValid&&f.speedKmh==0&&f.quality==2);
  const double held=f.latitude;
  // Valid but small Doppler/coordinate fluctuations remain stationary.
  for(uint32_t t=2300;t<10000;t+=200) {
    f=filter.apply(sample(t,1.2,(t%600)/400.0),t);
    assert(f.isValid&&f.speedKmh==0&&f.latitude==held);
  }
  // Even a 6 km/h speed indication alone cannot unlock an unmoving coordinate.
  for(uint32_t t=10100;t<14000;t+=200) {
    f=filter.apply(sample(t,6,1),t);assert(f.speedKmh==0&&f.latitude==held);
  }
  // Slow riding exits hold once displacement and sustained speed agree.
  for(uint32_t t=14100;t<=20100;t+=200)f=filter.apply(sample(t,4,(t-14100)/900.0),t);
  assert(f.isValid&&f.speedKmh==4&&f.latitude!=held);
  // Braking settles within 1.5 s; no residual smoothed speed after stopping.
  for(uint32_t t=20300;t<=22500;t+=200)f=filter.apply(sample(t,0.4,7),t);
  assert(f.isValid&&f.speedKmh==0);
  auto stopped=f.latitude;
  for(uint32_t t=22700;t<=24500;t+=200) {
    f=filter.apply(sample(t,0.7,7.5),t);assert(f.speedKmh==0&&f.latitude==stopped);
  }
  f=filter.apply(sample(24700,20,1000),24700);assert(!f.isValid); // teleport
  filter.reset();for(uint32_t t=100;t<=2300;t+=200)f=filter.apply(sample(t),t);
  f=filter.apply(sample(2500,90),2500);assert(!f.isValid&&f.speedKmh==0); // acceleration spike
  filter.reset();for(uint32_t t=100;t<=2300;t+=200)f=filter.apply(sample(t),t);
  raw=sample(2300);f=filter.apply(raw,4000);assert(!f.isValid&&f.quality==0);
  assert(!filter.apply(sample(4100),4100).isValid); // reacquire, no bridge across loss
  raw=sample(4300);raw.latitude=std::numeric_limits<double>::quiet_NaN();
  assert(!filter.apply(raw,4300).isValid);
  filter.reset();uint32_t begin=UINT32_MAX-1000;
  for(uint32_t i=0;i<=2200;i+=200)f=filter.apply(sample(begin+i),begin+i);
  assert(f.isValid); // millis wrap
  filter.reset();
  for(uint32_t t=100;t<=2300;t+=200) {
    raw=sample(t);raw.accuracyValid=false;raw.satellites=8;raw.hdop=1.2;
    f=filter.apply(raw,t);
  }
  assert(f.isValid&&f.speedKmh==0&&!f.accuracyValid);
  // Continuous riding with a right-angle turn stays usable; no position EMA
  // cuts the corner or retains a long speed tail after braking.
  filter.reset();
  for(uint32_t t=100;t<=10100;t+=200) {
    raw=sample(t,20,std::min(t-100,5000u)/180.0);
    if(t>5100)raw.longitude+=(t-5100)/180.0/110544.0;
    f=filter.apply(raw,t);
    if(t>=4300)assert(f.isValid&&f.speedKmh==20);
  }
  puts("PVT wire decoding, quality gates, stationary hold, slow start/stop, outliers and recovery passed.");
}
