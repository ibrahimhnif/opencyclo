#pragma once
#include <stdint.h>

// Boot/missing-card recovery only. Never unmount a working card or open GPX.
class SdMountPolicy {
  bool attempted=false,mounted=false;
  uint32_t lastAttempt=0;
  unsigned failures=0;
public:
  template<class Mount>
  bool tick(uint32_t now,bool blocked,Mount mount) {
    if(mounted || blocked || (attempted && now-lastAttempt<5000))return false;
    attempted=true;lastAttempt=now;
    // 4-bit 20 MHz -> 4-bit 10 MHz -> 1-bit 10 MHz for further retries.
    const bool oneBit=failures>=2;
    const int khz=failures==0?20000:10000;
    mounted=mount(oneBit,khz);
    if(!mounted && failures<2)++failures;
    return mounted; // true only on the successful transition.
  }
};
