#include "storage/sd_mount_policy.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <utility>
int main() {
  SdMountPolicy p;std::vector<std::pair<bool,int>> attempts;bool success=false;
  auto mount=[&](bool one,int khz){attempts.emplace_back(one,khz);return success;};
  assert(!p.tick(0,true,mount)&&attempts.empty());
  assert(!p.tick(1,false,mount)&&attempts.size()==1);
  assert(!attempts.back().first&&attempts.back().second==20000);
  assert(!p.tick(5000,false,mount)&&attempts.size()==1);
  assert(!p.tick(5001,false,mount)&&attempts.size()==2);
  assert(!attempts.back().first&&attempts.back().second==10000);
  assert(!p.tick(10001,true,mount)&&attempts.size()==2);
  assert(!p.tick(10002,false,mount)&&attempts.size()==3);
  assert(attempts.back().first&&attempts.back().second==10000);
  success=true;assert(p.tick(15002,false,mount));
  assert(!p.tick(25002,false,mount)&&attempts.size()==4); // Never remount a working card.
  SdMountPolicy wrap;success=false;
  assert(!wrap.tick(UINT32_MAX-1000,false,mount));size_t before=attempts.size();
  assert(!wrap.tick(3998,false,mount)&&attempts.size()==before);
  assert(!wrap.tick(3999,false,mount)&&attempts.size()==before+1);
  puts("SD mount fallback, retry spacing, shutdown/file exclusion and clock wrap passed.");
}
