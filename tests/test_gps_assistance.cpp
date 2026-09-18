#include "hardware/gps_assistance.h"
#include "hardware/gps_identity.h"
#include <cassert>
#include <vector>
#include <cstdio>
using namespace gnss;
struct FakePort : Port {
  uint32_t time=0;bool works=true;std::vector<uint8_t> sent;
  uint32_t now() override{return time;}
  void wait(uint32_t n) override{time+=n;}
  int read() override{return -1;}
  bool write(const uint8_t* p,size_t n) override{sent.assign(p,p+n);return works;}
};
static void cmd(Assistance& a,std::vector<uint8_t> b,uint32_t now=0){a.command(b.data(),b.size(),now);}
static std::vector<uint8_t> frame(uint8_t id,uint8_t type,size_t length) {
  std::vector<uint8_t> p(length),b(length+8);p[0]=type;
  packet(0x13,id,p.data(),length,b.data());return b;
}
static void ready(Assistance& a,FakePort& p) {
  cmd(a,{1,1,0,0,0});a.tick(p,true);assert(a.state==Assistance::ConfigAck);
  Frame ack;ack.cls=5;ack.id=1;ack.length=2;ack.payload[0]=6;ack.payload[1]=0x8a;
  a.receive(ack);assert(a.state==Assistance::Ready);
}
static void upload(Assistance& a,const std::vector<uint8_t>& f) {
  std::vector<uint8_t> b={2,1,0,0,0,uint8_t(a.index),0,0,0};
  b.insert(b.end(),f.begin(),f.end());cmd(a,b);cmd(a,b); // identical retry
  assert(a.used==f.size());cmd(a,{3,1,0,0,0,uint8_t(a.index),0});
}
static Frame acknowledgement(const std::vector<uint8_t>& f) {
  Frame a;a.cls=0x13;a.id=0x60;a.length=8;a.payload[0]=1;a.payload[3]=f[3];
  memcpy(a.payload+4,f.data()+6,4);return a;
}
int main() {
  FakePort port;Assistance a;ready(a,port);
  auto time=frame(0x40,0x10,24), eph=frame(0,1,68);
  assert(Assistance::valid(time.data(),time.size(),true));
  assert(!Assistance::valid(eph.data(),eph.size(),true));
  auto bad=time;bad.back()^=1;assert(!Assistance::valid(bad.data(),bad.size(),true));
  auto reset=frame(4,0,4);assert(!Assistance::valid(reset.data(),reset.size(),false));
  upload(a,time);a.tick(port,true);assert(a.state==Assistance::MessageAck);
  Frame wrong=acknowledgement(eph);a.receive(wrong);assert(a.index==0);
  a.receive(acknowledgement(time));assert(a.index==1 && a.used==0);
  cmd(a,{3,1,0,0,0,0,0});assert(a.state==Assistance::Ready);
  upload(a,eph);a.tick(port,true);a.receive(acknowledgement(eph));
  cmd(a,{4,1,0,0,0,2,0});assert(a.state==Assistance::Done);
  cmd(a,{5,1,0,0,0});assert(a.state==Assistance::Done);
  uint8_t status[12];a.status(status);assert(status[0]==1 && status[8]==2);
  a=Assistance();cmd(a,{1,1,0,0,0});a.tick(port,false);assert(a.error==Assistance::Unsupported);
  a=Assistance();ready(a,port);upload(a,time);a.tick(port,true);
  auto nack=acknowledgement(time);nack.payload[0]=0;nack.payload[2]=5;a.receive(nack);
  assert(a.error==Assistance::Rejected && a.receiverInfo==5);
  a=Assistance();ready(a,port);upload(a,time);a.tick(port,true);port.time=2001;a.tick(port,true);
  assert(a.error==Assistance::Timeout);
  port.time=0;a=Assistance();ready(a,port);cmd(a,{2,1,0,0,0,0,0,1,0,0});
  assert(a.error==Assistance::Invalid);
  a=Assistance();ready(a,port);cmd(a,{5,2,0,0,0});assert(a.busy());
  cmd(a,{5,1,0,0,0});assert(a.error==Assistance::Cancelled);
  a=Assistance();ready(a,port);port.time=15001;a.tick(port,true);assert(a.error==Assistance::Timeout);
  puts("GPS assistance tests passed");
  Identity ident;const uint8_t begin[]={1,9,0,0,0};ident.command(begin,5);
  port.time=0;ident.tick(port);assert(ident.state==Identity::WaitVersion);
  assert(port.sent[2]==0x0a && port.sent[3]==4 && port.sent.size()==8);
  Frame ver;ver.cls=0x0a;ver.id=4;ver.length=70;
  memcpy(ver.payload+40,"PROTVER=34.10",13);ident.receive(ver);
  assert(ident.state==Identity::PollId);ident.tick(port);
  assert(port.sent[2]==0x27 && port.sent[3]==3);
  Frame uid;uid.cls=0x27;uid.id=3;uid.length=10;uid.payload[0]=2;
  ident.receive(uid);assert(ident.state==Identity::Done && ident.total==98);
  uint8_t page[20];assert(ident.read(page)==20 && page[9]==98);
  const uint8_t select[]={2,9,0,0,0,95,0};ident.command(select,7);
  assert(ident.read(page)==14 && page[7]==95);
  const uint8_t outside[]={2,9,0,0,0,255,255};ident.command(outside,7);
  assert(ident.read(page)==11);
  ident=Identity();ident.command(begin,5);ident.tick(port);port.time=2001;ident.tick(port);
  assert(ident.state==Identity::Error && ident.error==1);
  ident=Identity();ident.command(begin,5);ident.cancel();assert(!ident.busy());
  puts("GPS identity polling, bounded pagination, timeout and cancellation passed");
}
