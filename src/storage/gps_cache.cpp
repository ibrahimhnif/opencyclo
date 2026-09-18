#include "gps_cache.h"
#include "gps_cache_policy.h"
#include "hardware/gps_assistance.h"
#include "sd_access.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <sys/time.h>
#include <mutex>
#include <atomic>

namespace {
std::mutex mutex;
uint8_t *active=nullptr,*incoming=nullptr;
uint32_t size=0,first=0,last=0,created=0,nonce=0,expected=0,received=0,checksum=0,touched=0;
uint8_t transfer=0; // 0 idle,1 receiving,2 saving,3 saved,4 error
uint8_t failure=0;
bool loaded=false;int slot=-1;
std::atomic<uint8_t> status{0}; // 0 missing,1 wait time,2 ready,3 applying,4 applied,5 expired,6 SD unavailable,7 error
std::atomic<uint32_t> publicNonce{0},publicReceived{0};
gnss::Assistance injector;
bool injecting=false;
size_t cursor=0;uint32_t appliedDay=0,attemptDay=0;
RTC_DATA_ATTR uint32_t clockSynced=0;
bool clockInit=false;
uint32_t nowUtc() {
  if(!clockInit){clockInit=true;if(esp_reset_reason()!=ESP_RST_DEEPSLEEP)clockSynced=0;}
  const time_t now=time(nullptr);
  if(!clockSynced || now<clockSynced || now-clockSynced>7*86400)return 0;
  return uint32_t(now);
}
void setClock(uint32_t utc) {
  if(utc<gpscache::epoch(2020,1,1) || utc>=gpscache::epoch(2099,1,1))return;
  nowUtc();timeval tv{time_t(utc),0};settimeofday(&tv,nullptr);clockSynced=utc;
}
uint8_t* allocate(){return static_cast<uint8_t*>(heap_caps_malloc(gpscache::capacity,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));}
const char* paths[]={"/gps-cache/predictive-a.bin","/gps-cache/predictive-b.bin"};
void discardIncoming(){if(incoming){free(incoming);incoming=nullptr;}}
void error(uint8_t e){transfer=4;failure=e;discardIncoming();}
bool loadSlot(int s,uint8_t* buffer,uint32_t& count,uint32_t& stamp,uint32_t& a,uint32_t& b) {
  File f=SD_MMC.open(paths[s],FILE_READ);if(!f)return false;
  uint32_t h[6]{};bool ok=f.read(reinterpret_cast<uint8_t*>(h),sizeof(h))==sizeof(h);
  count=h[1];stamp=h[3];
  ok=ok && h[0]==0x31435047 && count<=gpscache::capacity && f.size()==sizeof(h)+count;
  if(ok)ok=f.read(buffer,count)==count;
  f.close();
  return ok && gpscache::crc(buffer,count)==h[2] && gpscache::validate(buffer,count,a,b) && a==h[4] && b==h[5];
}
}

void gpsCacheCommand(const uint8_t* p,size_t n) {
  if(n<5)return;
  std::unique_lock<std::mutex> guard(mutex,std::try_to_lock);if(!guard.owns_lock())return;
  const uint32_t token=gnss::Assistance::u32(p+1);
  if(p[0]==1 && n==13) {
    if(injecting || transfer==2 || !token || (transfer==1 && token!=nonce))return;
    if(token==nonce)return;
    discardIncoming();nonce=token;expected=gnss::Assistance::u32(p+5);checksum=gnss::Assistance::u32(p+9);
    received=0;publicReceived=0;publicNonce=nonce;failure=0;touched=millis();
    if(!expected || expected>gpscache::capacity || expected%84){error(1);return;}
    incoming=allocate();if(!incoming){error(2);return;}transfer=1;return;
  }
  // Current UTC from a fresh HTTPS response, never restored from a cache file.
  if(p[0]==4 && n==9 && token==nonce && transfer==3) {setClock(gnss::Assistance::u32(p+5));return;}
  if(token!=nonce || transfer!=1)return;
  touched=millis();
  if(p[0]==5 && n==5){error(3);return;}
  if(p[0]==2 && n>9) {
    const size_t offset=gnss::Assistance::u32(p+5),count=n-9;
    if(offset+count>expected){error(1);return;}
    if(offset<received){if(offset+count>received || memcmp(incoming+offset,p+9,count))error(1);return;}
    if(offset!=received){error(1);return;}memcpy(incoming+offset,p+9,count);received+=count;publicReceived=received;
  } else if(p[0]==3 && n==5) {
    if(received!=expected || gpscache::crc(incoming,expected)!=checksum){error(1);return;}
    uint32_t a,b;if(!gpscache::validate(incoming,expected,a,b)){error(1);return;}
    transfer=2;
  }
}
void gpsCacheStatus(uint8_t out[20]) {
  std::unique_lock<std::mutex> guard(mutex,std::try_to_lock);
  if(!guard.owns_lock()) {
    // File I/O may be slow: never stall the NimBLE host callback on SD.
    memset(out,0,20);out[0]=1;out[1]=2;out[2]=status.load();
    const uint32_t fields[]={publicNonce.load(),publicReceived.load()};
    for(unsigned j=0;j<2;j++)for(unsigned k=0;k<4;k++)out[4+j*4+k]=uint8_t(fields[j]>>(8*k));
    return;
  }
  out[0]=1;out[1]=transfer;out[2]=status.load();out[3]=failure;
  const uint32_t fields[]={nonce,received,first,last};
  for(unsigned j=0;j<4;j++)for(unsigned k=0;k<4;k++)out[4+j*4+k]=uint8_t(fields[j]>>(8*k));
}
const char* gpsCacheLabel() {
  const char* labels[]={"perlu sync","menunggu waktu","cache siap","memakai cache","cache dipakai","perlu sync","SD unavailable","cache error"};
  return labels[status.load()];
}
bool gpsCacheInjecting(){std::unique_lock<std::mutex> g(mutex,std::try_to_lock);return !g.owns_lock() || injecting;}
void gpsCacheCancel(){std::lock_guard<std::mutex> g(mutex);if(injecting)injector.fail(gnss::Assistance::Cancelled);injecting=false;}

void gpsCacheStorageTick(bool mounted) {
  // Called only by LoggerTask under SdGuard. Bounded file sizes; no UART here.
  std::lock_guard<std::mutex> guard(mutex);
  if(transfer==1 && millis()-touched>15000)error(3);
  if(!mounted){status=6;if(transfer==2)error(4);return;}
  if(!loaded) {
    loaded=true;uint8_t* temp=allocate();if(!temp){status=7;return;}
    for(int s=0;s<2;s++) {
      uint32_t n,stamp,a,b;
      if(loadSlot(s,temp,n,stamp,a,b) && (slot<0 || stamp>created)) {
        if(!active)active=allocate();if(!active){free(temp);status=7;return;}
        memcpy(active,temp,n);size=n;created=stamp;first=a;last=b;slot=s;
      }
    }
    free(temp);
  }
  if(transfer==2) {
    uint32_t a,b;if(!gpscache::validate(incoming,expected,a,b)){error(1);return;}
    SD_MMC.mkdir("/gps-cache");const int target=slot==0?1:0;
    const uint32_t generation=created+1;
    uint32_t header[]={0x31435047,expected,checksum,generation,a,b};
    File f=SD_MMC.open(paths[target],"w");
    bool ok=bool(f);
    if(ok)ok=f.write(reinterpret_cast<uint8_t*>(header),sizeof(header))==sizeof(header) && f.write(incoming,expected)==expected;
    if(f){f.flush();f.close();}
    // Read-back CRC before reporting persistent success. The previous slot stays intact.
    uint8_t* check=allocate();uint32_t n,stamp,x,y;
    ok=ok && check && loadSlot(target,check,n,stamp,x,y) && n==expected && stamp==generation;
    if(check)free(check);
    if(!ok){error(4);return;}
    if(active)free(active);active=incoming;incoming=nullptr;
    size=expected;first=a;last=b;slot=target;created=generation;loaded=true;appliedDay=attemptDay=0;transfer=3;
  }
}

void gpsCacheReceive(const gnss::Frame& f) {
  std::unique_lock<std::mutex> guard(mutex,std::try_to_lock);if(!guard.owns_lock())return;
  // Receiver UTC must be date/time-valid and fully resolved (not a stale fix).
  if(f.cls==1 && f.id==7 && f.length==92 && (f.payload[11]&7)==7 &&
     gnss::Assistance::u32(f.payload+12)<1000000000u) {
    setClock(gpscache::epoch(gnss::Assistance::u16(f.payload+4),f.payload[6],f.payload[7],f.payload[8],f.payload[9],f.payload[10]));
  }
  if(injecting)injector.receive(f);
}
void gpsCacheGpsTick(gnss::Port& port,bool supported,bool occupied) {
  std::unique_lock<std::mutex> guard(mutex,std::try_to_lock);if(!guard.owns_lock())return;
  const uint32_t utc=nowUtc(),day=utc/86400;
  const uint32_t uncertainty=60+(utc>=clockSynced?(utc-clockSynced)/20:0);
  if(!injecting) {
    if(!active){if(status!=6)status=0;return;}
    if(!utc){status=1;return;}
    if(uncertainty>3600 || (utc-uncertainty)/86400!=(utc+uncertainty)/86400){status=1;return;}
    if(day<first || day>last){status=5;return;}
    if(appliedDay==day){status=4;return;}
    if(attemptDay==day){status=7;return;}
    status=2;if(occupied || incoming || !supported)return;
    // A hole in an otherwise valid bundle must not be reported as usable.
    bool found=false;for(size_t i=0;i<size;i+=84)if(gpscache::day(active+i)==day)found=true;
    if(!found){status=5;return;}
    injector=gnss::Assistance();injector.token=1;injector.state=gnss::Assistance::Configure;
    injector.activity=port.now();cursor=0;injecting=true;attemptDay=day;status=3;
  }
  if(!utc || day!=attemptDay){injector.fail(gnss::Assistance::Timeout);}
  if(injector.state==gnss::Assistance::Ready) {
    injector.activity=port.now();
    if(injector.index==0) {
      time_t value=utc;tm date{};gmtime_r(&value,&date);uint8_t p[24]{};
      p[0]=0x10;p[3]=0x80;p[4]=uint8_t(date.tm_year+1900);p[5]=uint8_t((date.tm_year+1900)>>8);
      p[6]=date.tm_mon+1;p[7]=date.tm_mday;p[8]=date.tm_hour;p[9]=date.tm_min;p[10]=date.tm_sec;
      p[16]=uint8_t(uncertainty);p[17]=uint8_t(uncertainty>>8);
      injector.used=gnss::packet(0x13,0x40,p,sizeof(p),injector.bytes);injector.state=gnss::Assistance::Send;
    } else {
      while(cursor<size && gpscache::day(active+cursor)!=day)cursor+=84;
      if(cursor==size){appliedDay=day;injecting=false;status=4;return;}
      memcpy(injector.bytes,active+cursor,84);injector.used=84;cursor+=84;injector.state=gnss::Assistance::Send;
    }
  }
  injector.tick(port,supported);
  if(injector.state==gnss::Assistance::Error){injecting=false;status=7;}
}
