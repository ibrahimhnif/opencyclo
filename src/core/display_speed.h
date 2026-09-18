#pragma once
#include <stdint.h>
#include <cmath>
// Presentation only: 3 s time-weighted window, published at 2 Hz. Raw speed
// remains the input to distance, ride state, max speed and diagnostics.
class DisplaySpeed {
  struct Sample { float speed; uint32_t time; } samples[64]{};
  unsigned head=0,count=0;
  uint32_t published=0;
  int source=-1;
  float shown=0;
public:
  float update(float speed,int nextSource,uint32_t now) {
    if(nextSource!=source || !std::isfinite(speed) || speed<=0 || nextSource==0) {
      count=0;head=0;shown=0;source=nextSource;
    }
    if(!std::isfinite(speed) || speed<=0 || nextSource==0)return 0;
    if(count && now-samples[(head+63)%64].time<50)return shown;
    samples[head]={speed,now};head=(head+1)%64;if(count<64)++count;
    if(count==1){shown=speed;published=now;return shown;}
    if(now-published<500)return shown;
    published=now;float weighted=0;uint32_t duration=0;
    for(unsigned age=1;age<count;age++) {
      const auto& older=samples[(head+63-age)%64];
      const auto& newer=samples[(head+64-age)%64];
      uint32_t a=now-older.time,b=now-newer.time;
      if(b>=3000)break;
      if(a>3000)a=3000;
      weighted+=older.speed*(a-b);duration+=a-b;
    }
    if(duration)shown=weighted/duration;
    return shown;
  }
};
