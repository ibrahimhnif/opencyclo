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
namespace fonts{constexpr int Font0=0,FreeSansBold9pt7b=1,FreeSansBold12pt7b=2,FreeSansBold24pt7b=3;}
struct Canvas {
  std::vector<std::string> labels;
  std::vector<std::string> commands;
  int font=0,cursorX=0,cursorY=0;uint16_t color=0;
  void fillScreen(uint16_t){labels.clear();commands.clear();}
  void setFont(const int* f){font=*f;}
  void setTextSize(int){}
  void setTextPadding(int){}
  void setTextColor(uint16_t c,uint16_t){color=c;}
  void drawString(const String& s,int x,int y){
    labels.push_back(s);char b[256];
    snprintf(b,sizeof(b),"T|%d|%d|%d|%u|%s",font,x,y,color,s.c_str());commands.push_back(b);
  }
  void fillRoundRect(int x,int y,int w,int h,int r,uint16_t c){
    char b[128];snprintf(b,sizeof(b),"R|%d|%d|%d|%d|%d|%u",x,y,w,h,r,c);commands.push_back(b);
  }
  void pushSprite(int,int){}
  void fillRect(int x,int y,int w,int h,uint16_t c){fillRoundRect(x,y,w,h,0,c);}
  void drawFastHLine(int x,int y,int w,uint16_t c){drawLine(x,y,x+w-1,y,c);}
  void setCursor(int x,int y){cursorX=x;cursorY=y;}
  void print(const char* s){drawString(s,cursorX,cursorY);}
  void setClipRect(int x,int y,int w,int h){char b[100];snprintf(b,sizeof(b),"C|%d|%d|%d|%d",x,y,w,h);commands.push_back(b);}
  void clearClipRect(){commands.push_back("U");}
  uint16_t color565(int r,int g,int b){return uint16_t((r>>3)<<11|(g>>2)<<5|(b>>3));}
  void drawLine(int x,int y,int xx,int yy,uint16_t c){
    char b[128];snprintf(b,sizeof(b),"L|%d|%d|%d|%d|%u",x,y,xx,yy,c);commands.push_back(b);
  }
};
extern Canvas canvas;
