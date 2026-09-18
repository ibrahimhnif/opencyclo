#pragma once
#include "gnss_power.h"

// One in-flight UBX frame; BLE never owns the UART. All methods are externally
// serialized. Wire integers are little endian. No arbitrary UBX passthrough.
namespace gnss {
class Assistance {
public:
  enum State : uint8_t { Idle, Configure, ConfigAck, Ready, Send, MessageAck, Done, Error };
  enum Failure : uint8_t { None, Unsupported, Invalid, Timeout, Rejected, Transport, Cancelled };
  State state=Idle;
  uint8_t error=None, receiverInfo=0;
  uint32_t token=0, deadline=0, activity=0;
  uint16_t index=0, used=0;
  uint8_t bytes[520]{};
  static uint16_t u16(const uint8_t* p){return p[0]|uint16_t(p[1])<<8;}
  static uint32_t u32(const uint8_t* p){return u16(p)|uint32_t(u16(p+2))<<16;}
  bool busy()const {return state>=Configure && state<=MessageAck;}
  void fail(uint8_t why){state=Error;error=why;}
  static bool valid(const uint8_t* p,size_t n,bool first) {
    if(n<12 || n>520 || p[0]!=0xb5 || p[1]!=0x62 || p[2]!=0x13 || u16(p+4)+8u!=n)return false;
    uint8_t a=0,b=0;for(size_t i=2;i<n-2;i++){a+=p[i];b+=a;}
    if(a!=p[n-2] || b!=p[n-1] || p[7]!=0)return false;
    if(first)return p[3]==0x40 && p[6]==0x10 && n==32 && p[8]==0;
    // Live GPS/Galileo only: ephemeris, UTC and GPS ionosphere.
    return (p[3]==0 && ((p[6]==1 && n==76)||(p[6]==4 && n==48)||(p[6]==5 && n==28)||(p[6]==6 && n==24))) ||
           (p[3]==2 && ((p[6]==1 && n==84)||(p[6]==5 && n==28)));
  }
  void command(const uint8_t* p,size_t n,uint32_t now) {
    if(n<5)return;
    const uint32_t t=u32(p+1);
    if(p[0]==1 && n==5) {
      if(t==0 || (busy() && t!=token))return;
      if(t==token && state!=Idle)return; // idempotent BEGIN, including errors
      token=t;index=used=0;error=receiverInfo=0;activity=now;state=Configure;return;
    }
    if(t!=token || !busy())return;
    activity=now;
    if(p[0]==5 && n==5){fail(Cancelled);return;}
    if(p[0]==4 && n==7 && state==Ready && used==0 && index>1 && u16(p+5)==index){state=Done;return;}
    if(n<7 || u16(p+5)!=index)return; // retries for an already ACKed frame
    if(p[0]==2 && n>9 && state==Ready) {
      const size_t offset=u16(p+7), count=n-9;
      if(offset+count>sizeof(bytes)){fail(Invalid);return;}
      if(offset<used) {if(offset+count>used || memcmp(bytes+offset,p+9,count))fail(Invalid);return;}
      if(offset!=used){fail(Invalid);return;}
      memcpy(bytes+used,p+9,count);used+=count;
    } else if(p[0]==3 && n==7 && state==Ready) {
      if(index>=256 || !valid(bytes,used,index==0)){fail(Invalid);return;}
      state=Send;
    }
  }
  void tick(Port& port,bool supported) {
    const uint32_t now=port.now();
    if(busy() && now-activity>15000){fail(Timeout);return;}
    if(state==Configure) {
      if(!supported){fail(Unsupported);return;}
      // RAM only: acknowledge MGA input, ensure UBX UART1 output. No reset,
      // rate, constellation, dynamic-model or flash configuration changes.
      const uint8_t cfg[]={0,1,0,0,0x25,0,0x11,0x10,1,1,0,0x74,0x10,1};
      uint8_t out[22];packet(6,0x8a,cfg,sizeof(cfg),out);
      if(!port.write(out,sizeof(out))){fail(Transport);return;}
      state=ConfigAck;deadline=now;
    } else if(state==Send) {
      if(!port.write(bytes,used)){fail(Transport);return;}
      state=MessageAck;deadline=now;
    } else if((state==ConfigAck || state==MessageAck) && now-deadline>2000)fail(Timeout);
  }
  void receive(const Frame& f) {
    if(state==ConfigAck && f.cls==5 && f.length==2 && f.payload[0]==6 && f.payload[1]==0x8a) {
      if(f.id==1)state=Ready;else if(f.id==0)fail(Rejected);
    } else if(state==MessageAck && f.cls==0x13 && f.id==0x60 && f.length==8 &&
              f.payload[1]==0 && f.payload[3]==bytes[3] && !memcmp(f.payload+4,bytes+6,4)) {
      receiverInfo=f.payload[2];
      if(f.payload[0]==1 && receiverInfo==0){index++;used=0;state=Ready;}
      else fail(Rejected);
    }
  }
  void status(uint8_t* out)const {
    out[0]=1;out[1]=state;out[2]=error;out[3]=receiverInfo;
    for(unsigned i=0;i<4;i++)out[4+i]=uint8_t(token>>(8*i));
    out[8]=uint8_t(index);out[9]=uint8_t(index>>8);
    out[10]=uint8_t(used);out[11]=uint8_t(used>>8);
  }
};
}
