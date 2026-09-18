#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <algorithm>
#include <ctime>
#include <sys/time.h>
extern uint32_t fakeMs;
inline uint32_t millis(){return fakeMs;}
#define RTC_DATA_ATTR
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
inline void* heap_caps_malloc(size_t n,int){return malloc(n);}
enum {ESP_RST_DEEPSLEEP=5};
inline int esp_reset_reason(){return 0;}
using SemaphoreHandle_t=void*;
#define pdMS_TO_TICKS(n) (n)
#define pdTRUE 1
inline int xSemaphoreTakeRecursive(void*,int){return 1;}
inline void xSemaphoreGiveRecursive(void*){}
extern size_t cacheWriteLimit;
struct File {
  std::string* value=nullptr;size_t at=0;
  explicit operator bool()const{return value;}
  size_t size()const{return value?value->size():0;}
  size_t read(uint8_t* p,size_t n){n=std::min(n,value->size()-at);memcpy(p,value->data()+at,n);at+=n;return n;}
  size_t write(const uint8_t* p,size_t n){n=std::min(n,cacheWriteLimit);value->append(reinterpret_cast<const char*>(p),n);return n;}
  void close(){value=nullptr;}
  void flush(){}
};
#define FILE_READ "r"
struct CacheCard {
  std::map<std::string,std::string> files;
  bool mkdir(const char*){return true;}
  File open(const char* path,const char* mode){File f;if(*mode=='r' && !files.count(path))return f;f.value=&files[path];if(*mode=='w')f.value->clear();return f;}
};
extern CacheCard SD_MMC;
extern time_t fakeUtc;
inline time_t cacheTime(time_t*){return fakeUtc;}
inline int cacheSetTime(const timeval* t,void*){fakeUtc=t->tv_sec;return 0;}
#define time cacheTime
#define settimeofday cacheSetTime
