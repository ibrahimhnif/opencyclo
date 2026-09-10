#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>
extern uint32_t fakeMillis;
inline uint32_t millis(){return fakeMillis;}
using SemaphoreHandle_t=std::mutex*;
constexpr int pdTRUE=1,portMAX_DELAY=-1;
inline int pdMS_TO_TICKS(int n){return n;}
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return new std::mutex;}
inline int xSemaphoreTake(SemaphoreHandle_t m,int){m->lock();return pdTRUE;}
inline void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
class String:public std::string {
public:
  using std::string::string;
  String(const std::string& s):std::string(s){}
  String substring(size_t a,size_t b=std::string::npos)const {
    if(a>size())return "";
    return substr(a,b==std::string::npos?b:b-a);
  }
};
constexpr uint16_t TFT_WHITE=0xffff,TFT_BLACK=0,TFT_CYAN=0x7ff,TFT_GREEN=0x7e0,TFT_ORANGE=0xfd20;
namespace fonts{constexpr int Font0=0;}
struct Canvas {
  std::vector<std::string> labels;
  void fillScreen(uint16_t){labels.clear();}
  void setFont(const int*){}
  void setTextSize(int){}
  void setTextPadding(int){}
  void setTextColor(uint16_t,uint16_t){}
  void drawString(const String& s,int,int){labels.push_back(s);}
  void fillRoundRect(int,int,int,int,int,uint16_t){}
  void pushSprite(int,int){}
};
extern Canvas canvas;
