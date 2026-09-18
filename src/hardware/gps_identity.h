#pragma once
#include "gps_assistance.h"

namespace gnss {
// Read-only receiver identity polling. No token/credential ever reaches UART.
class Identity {
public:
  enum State : uint8_t {Idle, PollVersion, WaitVersion, PollId, WaitId, Done, Error};
  State state=Idle;
  uint32_t token=0,started=0;
  uint16_t total=0,offset=0;
  uint8_t error=0, data[540]{};
  bool busy()const{return state>=PollVersion && state<=WaitId;}
  void cancel(){state=Error;error=3;total=0;}
  void command(const uint8_t* p,size_t n) {
    if(n<5)return;
    const uint32_t t=Assistance::u32(p+1);
    if(p[0]==1 && n==5 && t && !busy() && t!=token) {
      token=t;total=offset=0;error=0;state=PollVersion;
    } else if(t==token && p[0]==2 && n==7)offset=Assistance::u16(p+5);
    else if(t==token && p[0]==3 && n==5 && busy())cancel();
  }
  void tick(Port& port) {
    if(state==PollVersion || state==PollId) {
      const bool version=state==PollVersion;
      uint8_t request[8];packet(version?0x0a:0x27,version?4:3,nullptr,0,request);
      if(!port.write(request,8)){state=Error;error=2;return;}
      started=port.now();state=version?WaitVersion:WaitId;
    } else if((state==WaitVersion || state==WaitId) && port.now()-started>2000) {
      state=Error;error=1;total=0;
    }
  }
  void receive(const Frame& f) {
    if(state==WaitVersion && f.cls==0x0a && f.id==4 && f.length>=40 &&
       f.length<=512 && (f.length-40)%30==0) {
      const size_t n=packet(f.cls,f.id,f.payload,f.length,data+2);
      data[0]=uint8_t(n);data[1]=uint8_t(n>>8);total=n+2;state=PollId;
    } else if(state==WaitId && f.cls==0x27 && f.id==3 &&
              ((f.length==9 && f.payload[0]==1)||(f.length==10 && f.payload[0]==2))) {
      total+=packet(f.cls,f.id,f.payload,f.length,data+total);state=Done;
    }
  }
  size_t read(uint8_t out[20])const {
    out[0]=1;out[1]=state;out[2]=error;
    for(unsigned i=0;i<4;i++)out[3+i]=uint8_t(token>>(8*i));
    out[7]=uint8_t(offset);out[8]=uint8_t(offset>>8);
    const uint16_t size=state==Done?total:0;
    out[9]=uint8_t(size);out[10]=uint8_t(size>>8);
    size_t n=offset<size?size-offset:0;if(n>9)n=9;
    if(n)memcpy(out+11,data+offset,n);
    return 11+n;
  }
};
}
