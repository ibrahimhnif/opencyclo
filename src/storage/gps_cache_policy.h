#pragma once
#include "hardware/gnss_power.h"
namespace gpscache {
constexpr size_t capacity=128*1024;
inline uint32_t crc(const uint8_t* p,size_t n) {
  uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int b=0;b<8;b++)c=(c>>1)^((c&1)?0xedb88320u:0);}return ~c;
}
inline uint32_t epoch(unsigned y,unsigned m,unsigned d,unsigned h=0,unsigned min=0,unsigned s=0) {
  if(y<2020 || y>2099 || m<1 || m>12 || d<1 || h>23 || min>59 || s>59)return 0;
  const unsigned lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
  if(d>lengths[m-1]+(m==2 && y%4==0))return 0;
  uint32_t days=18262;for(unsigned a=2020;a<y;a++)days+=365+(a%4==0);
  for(unsigned a=1;a<m;a++)days+=lengths[a-1]+(a==2 && y%4==0);
  return ((days+d-1)*24+h)*3600+min*60+s;
}
inline uint32_t day(const uint8_t* f){return epoch(2000+f[10],f[11],f[12])/86400;}
inline bool validate(const uint8_t* p,size_t n,uint32_t& first,uint32_t& last) {
  if(!n || n>capacity || n%84)return false;
  first=~0u;last=0;
  for(size_t i=0;i<n;i+=84) {
    const uint8_t* f=p+i;
    if(f[0]!=0xb5 || f[1]!=0x62 || f[2]!=0x13 || f[3]!=0x20 || f[4]!=76 || f[5] || f[6] || f[7] || (f[9]!=0 && f[9]!=2))return false;
    uint8_t a=0,b=0;for(unsigned j=2;j<82;j++){a+=f[j];b+=a;}
    const uint32_t date=day(f);if(!date || a!=f[82] || b!=f[83])return false;
    if(date<first)first=date;if(date>last)last=date;
  }
  return last-first<=6; // seven calendar days, no timeless or arbitrary UBX cache
}
}
