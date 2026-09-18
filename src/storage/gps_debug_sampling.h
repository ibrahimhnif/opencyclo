#pragma once
#include <stdint.h>
#include <cstring>

// Single GPS producer only. Decimate diagnostics, never receiver processing.
class GpsDebugSampling {
  struct Slot {uint32_t key=0,at=0;bool used=false;};
  Slot slots[64];
  bool haveDecision=false;
  uint32_t decisionAt=0,decisionKey=0;
  static uint32_t hash(uint32_t h,char c){return (h^uint8_t(c))*16777619u;}
public:
  uint32_t sampledOut=0;
  bool decision(uint32_t now,const char* reason,unsigned sats,unsigned type,bool valid) {
    uint32_t key=2166136261u;
    for(const char* p=reason;*p;++p)key=hash(key,*p);
    key=hash(hash(hash(key,char(sats)),char(type)),char(valid));
    // Keep state changes immediately; otherwise at most five samples/second.
    if(haveDecision && key==decisionKey && now-decisionAt<200){++sampledOut;return false;}
    haveDecision=true;decisionAt=now;decisionKey=key;return true;
  }
  bool nmea(uint32_t now,const char* line) {
    // Key by talker/type and GSV page + signal, or GSA system ID.
    // Do not key by signal strength: that changes every observation.
    unsigned fields=1;for(const char* p=line;*p&&*p!='*';++p)if(*p==',')++fields;
    const bool gsv=std::strlen(line)>=6 && !std::strncmp(line+3,"GSV",3);
    const bool gsa=std::strlen(line)>=6 && !std::strncmp(line+3,"GSA",3);
    uint32_t key=2166136261u;unsigned field=0;
    for(const char* p=line;*p&&*p!='*';++p) {
      if(*p==','){++field;continue;}
      if(field==0 || (gsv && (field==2 || (fields%4==1 && field==fields-1))) ||
         (gsa && fields==19 && field==18))key=hash(key,*p);
    }
    Slot* empty=nullptr;
    for(auto& s:slots) {
      if(s.used && s.key==key) {
        if(now-s.at<1000){++sampledOut;return false;}
        s.at=now;return true;
      }
      if(!s.used || now-s.at>=1000)empty=&s;
    }
    if(!empty){++sampledOut;return false;}
    empty->used=true;empty->key=key;empty->at=now;return true;
  }
};
