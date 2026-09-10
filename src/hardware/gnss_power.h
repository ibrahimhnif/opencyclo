#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// u-blox M10 SPG 5.10, protocol 34.10. No reset/backup-clear commands.
// Bounded parser also accepts interleaved NMEA, ignoring it during transactions.
namespace gnss {
struct Frame {
  uint8_t cls=0,id=0,payload[512]{};
  uint16_t length=0;
};
class Parser {
  uint8_t state=0,a=0,b=0;
  uint16_t pos=0;
  void sum(uint8_t c){a+=c;b+=a;}
public:
  Frame frame;
  bool feed(uint8_t c) {
    switch(state) {
      case 0: if(c==0xb5)state=1;break;
      case 1: state=c==0x62?2:(c==0xb5?1:0);a=b=0;break;
      case 2: frame.cls=c;sum(c);state=3;break;
      case 3: frame.id=c;sum(c);state=4;break;
      case 4: frame.length=c;sum(c);state=5;break;
      case 5:
        frame.length|=uint16_t(c)<<8;sum(c);pos=0;
        state=frame.length>sizeof(frame.payload)?0:(frame.length?6:7);break;
      case 6: frame.payload[pos++]=c;sum(c);if(pos==frame.length)state=7;break;
      case 7: state=c==a?8:0;break;
      case 8: state=0;return c==b;
    }
    return false;
  }
};
inline size_t packet(uint8_t cls,uint8_t id,const uint8_t* p,size_t n,uint8_t* out) {
  out[0]=0xb5;out[1]=0x62;out[2]=cls;out[3]=id;
  out[4]=uint8_t(n);out[5]=uint8_t(n>>8);
  if(n)memcpy(out+6,p,n);
  uint8_t a=0,b=0;
  for(size_t i=2;i<6+n;i++){a+=out[i];b+=a;}
  out[6+n]=a;out[7+n]=b;return n+8;
}
inline bool supportedVersion(const Frame& f) {
  if(f.cls!=0x0a || f.id!=4 || f.length<40 || (f.length-40)%30)return false;
  for(size_t i=40;i+30<=f.length;i+=30)
    if(!memcmp(f.payload+i,"PROTVER=34.10",13) && f.payload[i+13]==0)return true;
  return false;
}
struct Port {
  virtual ~Port()=default;
  virtual uint32_t now()=0;
  virtual void wait(uint32_t ms)=0;
  virtual int read()=0; // -1 when no data
  virtual bool write(const uint8_t*,size_t)=0; // Includes bounded TX completion.
};
enum class Standby { Quiet, Unsupported, NoResponse, WriteFailed, StillTransmitting };
class Power {
  Port& port;
  Parser parser;
  bool known=false;
  bool send(uint8_t cls,uint8_t id,const uint8_t* p,size_t n) {
    uint8_t bytes[40];if(n>32)return false;
    return port.write(bytes,packet(cls,id,p,n,bytes));
  }
  void discard() {
    for(unsigned i=0;i<4096 && port.read()>=0;i++){}
    parser=Parser();
  }
public:
  char software[31]{},hardware[11]{},protocol[31]{};
  explicit Power(Port& p):port(p){}
  bool supported()const{return known;}
  bool identify() {
    known=false;software[0]=hardware[0]=protocol[0]=0;discard();
    if(!send(0x0a,4,nullptr,0))return false;
    const uint32_t started=port.now();
    while(port.now()-started<1200) {
      for(unsigned i=0;i<256;i++) {
        int c=port.read();if(c<0)break;
        if(parser.feed(uint8_t(c)) && parser.frame.cls==0x0a && parser.frame.id==4) {
          const auto& f=parser.frame;
          if(f.length<40 || (f.length-40)%30)continue;
          memcpy(software,f.payload,30);software[30]=0;
          memcpy(hardware,f.payload+30,10);hardware[10]=0;
          for(size_t j=40;j+30<=f.length;j+=30) {
            if(!memcmp(f.payload+j,"PROTVER=",8)) {
              memcpy(protocol,f.payload+j,30);protocol[30]=0;
            }
          }
          known=supportedVersion(parser.frame);return true;
        }
      }
      port.wait(5);
    }
    return false;
  }
  bool wake() {
    const uint8_t edge=0xff;
    if(!port.write(&edge,1))return false;
    port.wait(500); // Wake byte is not a UBX command; wait before polling.
    return identify();
  }
  bool configure() {
    if(!known)return false;
    // CFG-VALSET, RAM only: 200 ms measurements, one navigation per
    // measurement, portable model. No flash wear or backup-data erasure.
    const uint8_t values[]={0,1,0,0,
      1,0,0x21,0x30,0xc8,0,
      2,0,0x21,0x30,1,0,
      0x21,0,0x11,0x20,0};
    discard();if(!send(6,0x8a,values,sizeof(values)))return false;
    const uint32_t started=port.now();
    while(port.now()-started<1200) {
      for(unsigned i=0;i<256;i++) {
        int c=port.read();if(c<0)break;
        if(parser.feed(uint8_t(c)) && parser.frame.cls==5 && parser.frame.length==2 &&
           parser.frame.payload[0]==6 && parser.frame.payload[1]==0x8a)
          return parser.frame.id==1;
      }
      port.wait(5);
    }
    return false;
  }
  Standby standby() {
    // Require a fresh valid reply before treating subsequent silence as useful
    // evidence. An unplugged/nonresponsive UART must not look like success.
    if(!identify())return Standby::NoResponse;
    if(!known)return Standby::Unsupported;
    const uint8_t request[]={0,0,0,0, 0,0,0,0, 6,0,0,0, 8,0,0,0};
    if(!send(2,0x41,request,sizeof(request)))return Standby::WriteFailed;
    const uint32_t started=port.now();uint32_t lastRx=started;
    while(port.now()-started<2500) {
      for(unsigned i=0;i<256;i++) {
        if(port.read()<0)break;
        lastRx=port.now();
      }
      // PMREQ is not acknowledged with CFG ACK. Silence is not a current
      // measurement; report exactly what was observed, not confirmed low power.
      if(port.now()-lastRx>=1200)return Standby::Quiet;
      port.wait(10);
    }
    return Standby::StillTransmitting;
  }
};
}
