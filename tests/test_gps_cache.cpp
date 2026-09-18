#include "cache_fakes.h"
#include "storage/gps_cache.cpp"
#include <cassert>
#include <cstdio>
#include <vector>
uint32_t fakeMs=0;time_t fakeUtc=0;size_t cacheWriteLimit=1000000;CacheCard SD_MMC;
SemaphoreHandle_t g_sd_mutex=nullptr;bool g_sd_ready=true;
struct CachePort:gnss::Port{
  std::vector<uint8_t> sent;
  uint32_t now()override{return fakeMs;}void wait(uint32_t n)override{fakeMs+=n;}
  int read()override{return -1;}
  bool write(const uint8_t* p,size_t n)override{sent.assign(p,p+n);return true;}
};
std::vector<uint8_t> bundle(unsigned d){uint8_t p[76]{};p[2]=1;p[4]=26;p[5]=9;p[6]=d;std::vector<uint8_t>b(84);gnss::packet(0x13,0x20,p,76,b.data());return b;}
void command(std::vector<uint8_t> b){gpsCacheCommand(b.data(),b.size());}
void put32(std::vector<uint8_t>& b,uint32_t n){for(int i=0;i<4;i++)b.push_back(n>>(8*i));}
void upload(uint32_t id,const std::vector<uint8_t>& b){
  std::vector<uint8_t> start{1};put32(start,id);put32(start,b.size());put32(start,gpscache::crc(b.data(),b.size()));command(start);
  std::vector<uint8_t> data{2};put32(data,id);put32(data,0);data.insert(data.end(),b.begin(),b.end());command(data);command(data);
  std::vector<uint8_t>end{3};put32(end,id);command(end);gpsCacheStorageTick(true);
}
int main(){
  const auto b=bundle(13);uint32_t a,z;
  assert(gpscache::epoch(2026,2,30)==0);
  assert(gpscache::epoch(2026,9,13)==1789257600);
  assert(gpscache::validate(b.data(),b.size(),a,z));
  auto bad=b;bad[20]^=1;assert(!gpscache::validate(bad.data(),bad.size(),a,z));
  CachePort port;gpsCacheStorageTick(true);upload(7,b);assert(transfer==3 && active && slot==0);
  gpsCacheGpsTick(port,true,false);assert(std::string(gpsCacheLabel())=="menunggu waktu");
  std::vector<uint8_t> clock{4};put32(clock,7);put32(clock,gpscache::epoch(2026,9,13,12));command(clock);
  gpsCacheGpsTick(port,true,false);assert(injecting && port.sent[2]==6);
  gnss::Frame ack;ack.cls=5;ack.id=1;ack.length=2;ack.payload[0]=6;ack.payload[1]=0x8a;gpsCacheReceive(ack);
  gpsCacheGpsTick(port,true,false);assert(port.sent[3]==0x40); // regenerated current UTC first
  auto accept=[&](){gnss::Frame r;r.cls=0x13;r.id=0x60;r.length=8;r.payload[0]=1;r.payload[3]=port.sent[3];memcpy(r.payload+4,port.sent.data()+6,4);gpsCacheReceive(r);};
  accept();gpsCacheGpsTick(port,true,false);assert(port.sent[3]==0x20);
  accept();gpsCacheGpsTick(port,true,false);assert(!injecting && appliedDay && status==4);
  // Failed update cannot replace previous verified slot.
  cacheWriteLimit=5;upload(8,bundle(14));assert(transfer==4 && slot==0);cacheWriteLimit=1000000;
  free(active);active=nullptr;size=created=0;slot=-1;loaded=false;appliedDay=attemptDay=0;
  gpsCacheStorageTick(true);assert(active && slot==0); // corrupt new slot ignored
  // Fresh known time outside bundle must request a sync, never inject.
  setClock(gpscache::epoch(2026,9,14,12));gpsCacheGpsTick(port,true,false);assert(status==5 && !injecting);
  clockSynced=0;gpsCacheGpsTick(port,true,false);assert(status==1);
  free(active);active=nullptr;
  puts("GPS cache: dates, CRC, persistent slots, failure rollback, clock gating and auto injection passed");
}
