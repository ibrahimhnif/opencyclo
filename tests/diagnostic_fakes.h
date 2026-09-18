#pragma once
#include "gps_fakes.h"
#include <map>
#include <string>
#include <algorithm>
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
struct DebugSerial {
  void println(const char*) {}
  template<class... T> void printf(const char*,T...) {}
};
extern DebugSerial Serial;
extern size_t writeLimit;
struct File {
  std::string* value=nullptr;
  explicit operator bool() const {return value;}
  size_t write(const uint8_t* p,size_t n) {n=std::min(n,writeLimit);value->append((const char*)p,n);return n;}
  void close(){value=nullptr;}
  void flush(){}
};
enum {FILE_WRITE=1};
struct DebugCard {
  std::map<std::string,std::string> files;
  bool exists(const char* p){return files.count(p);}
  bool mkdir(const char*){return true;}
  File open(const char* p,int){File f;f.value=&files[p];f.value->clear();return f;}
};
extern DebugCard SD_MMC;
