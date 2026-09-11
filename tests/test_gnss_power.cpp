#include "hardware/gnss_power.h"
#include <cassert>
#include <deque>
#include <vector>
#include <cstdio>

struct FakePort : gnss::Port {
  uint32_t clock=0;
  bool respond=true,known=true,corrupt=false,ack=true,failWrite=false;
  bool noisy=false,requested=false;
  bool failStandbyWrite=false;
  std::deque<uint8_t> rx;
  std::vector<std::vector<uint8_t>> writes;
  uint32_t now()override{return clock;}
  void wait(uint32_t ms)override{clock+=ms;}
  int read()override {
    if(rx.empty())return noisy&&requested?'$':-1;
    int c=rx.front();rx.pop_front();return c;
  }
  void reply(uint8_t cls,uint8_t id,const uint8_t* p,size_t n) {
    uint8_t bytes[600];const auto len=gnss::packet(cls,id,p,n,bytes);
    if(corrupt)bytes[len-1]^=1;
    rx.insert(rx.end(),bytes,bytes+len);
  }
  bool write(const uint8_t* p,size_t n)override {
    if(failWrite)return false;
    if(failStandbyWrite && n>3 && p[2]==2 && p[3]==0x41)return false;
    writes.emplace_back(p,p+n);
    if(n==1) {assert(p[0]==0xff);requested=false;return true;}
    gnss::Parser parser;bool complete=false;
    for(size_t i=0;i<n;i++)complete=parser.feed(p[i]);
    assert(complete);
    auto& f=parser.frame;
    assert(!(f.cls==6 && f.id==4)); // Never send CFG-RST / clear backup data.
    if(f.cls==0x0a && f.id==4 && respond) {
      uint8_t version[70]{};memcpy(version,"SPG 5.10",8);
      memcpy(version+40,known?"PROTVER=34.10":"PROTVER=34.00",13);
      reply(0x0a,4,version,sizeof(version));
    } else if(f.cls==6 && f.id==0x8a) {
      assert(f.length==31 && f.payload[1]==1); // RAM only.
      const uint8_t keys[]={0,1,0,0,1,0,0x21,0x30,200,0,
        2,0,0x21,0x30,1,0,0x21,0,0x11,0x20,0,
        7,0,0x91,0x20,1,1,0,0x74,0x10,1};
      assert(!memcmp(f.payload,keys,sizeof(keys)));
      const uint8_t target[]={6,0x8a};reply(5,ack?1:0,target,2);
    } else if(f.cls==2 && f.id==0x41) {
      const uint8_t expected[]={0,0,0,0,0,0,0,0,6,0,0,0,8,0,0,0};
      assert(f.length==16 && !memcmp(f.payload,expected,16));requested=true;
    }
    return true;
  }
};
int main() {
  FakePort p;gnss::Power power(p);
  assert(power.wake() && power.supported());
  assert(!strcmp(power.protocol,"PROTVER=34.10") && !strcmp(power.software,"SPG 5.10"));
  assert(p.writes[0]==std::vector<uint8_t>{0xff});
  assert(power.configure());
  assert(power.standby()==gnss::Standby::Quiet && p.requested);
  assert(power.wake() && !p.requested && power.configure());
  p.ack=false;assert(!power.configure());

  FakePort unknown;unknown.known=false;gnss::Power other(unknown);
  assert(other.wake() && !other.supported() && !other.configure());
  assert(other.standby()==gnss::Standby::Unsupported && !unknown.requested);
  FakePort missing;missing.respond=false;gnss::Power absent(missing);
  assert(!absent.wake());
  assert(absent.standby()==gnss::Standby::NoResponse && !missing.requested);
  assert(missing.clock<4000);
  FakePort bad;bad.corrupt=true;gnss::Power broken(bad);
  assert(!broken.wake() && !broken.supported());
  FakePort active;active.noisy=true;gnss::Power ignored(active);
  assert(ignored.standby()==gnss::Standby::StillTransmitting);
  assert(active.clock<=3700);
  FakePort tx;tx.failWrite=true;gnss::Power disconnected(tx);assert(!disconnected.wake());
  FakePort partial;partial.failStandbyWrite=true;gnss::Power failedCommand(partial);
  assert(failedCommand.standby()==gnss::Standby::WriteFailed && !partial.requested);

  // Oversized frame and noise cannot overflow or poison the next valid poll.
  gnss::Parser parser;
  const uint8_t noise[]={0x24,0xb5,0x62,0x0a,4,0xff,0xff};
  for(auto c:noise)assert(!parser.feed(c));
  uint8_t bytes[8];gnss::packet(0x0a,4,nullptr,0,bytes);
  for(size_t i=0;i<7;i++)assert(!parser.feed(bytes[i]));
  assert(parser.feed(bytes[7]));
  puts("GNSS version/checksum, RAM config ACK, standby, UART wake and timeout tests passed");
}
