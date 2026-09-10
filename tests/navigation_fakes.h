#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstdio>

class String : public std::string {
public:
  using std::string::string;
  String(const std::string& s):std::string(s){}
  String(int n):std::string(std::to_string(n)){}
  String(unsigned n):std::string(std::to_string(n)){}
  String(double n,int precision):std::string() {char b[64];snprintf(b,64,"%.*f",precision,n);assign(b);}
  bool startsWith(const char* s) const{return rfind(s,0)==0;}
  bool endsWith(const char* s) const{return size()>=strlen(s)&&compare(size()-strlen(s),strlen(s),s)==0;}
  String substring(size_t a,size_t b=std::string::npos) const{return substr(a,b==std::string::npos?b:b-a);}
};
extern uint32_t fakeMillis;
extern bool failMemory, syncOwned, updating;
inline uint32_t millis(){return fakeMillis;}
inline void* ps_malloc(size_t n){return failMemory?nullptr:malloc(n);}
enum {RIDE_STATE_IDLE,RIDE_STATE_ACTIVE,RIDE_STATE_PAUSED};
struct TelemetryState {double lat=0,lon=0;bool gps_has_fix=false;int ride_state=0;};
extern bool fakeSdBusy;
struct SdGuard {explicit SdGuard(uint32_t=1000):locked(!fakeSdBusy){} bool locked;};
extern bool g_sd_ready;
struct FakeFile {std::vector<uint8_t> bytes;bool directory=false;};
extern std::map<std::string,std::shared_ptr<FakeFile>> fs;
constexpr int FILE_WRITE=1;
class File {
  std::shared_ptr<FakeFile> file;
  std::string path;
  size_t pos=0,index=0;
public:
  File()=default;
  File(std::string p,std::shared_ptr<FakeFile> f):file(f),path(p){}
  explicit operator bool() const{return bool(file);}
  size_t size() const{return file?file->bytes.size():0;}
  size_t read(uint8_t* out,size_t n){if(!file)return 0;n=std::min(n,size()-pos);if(n)memcpy(out,file->bytes.data()+pos,n);pos+=n;return n;}
  size_t write(const uint8_t* in,size_t n){if(!file)return 0;file->bytes.insert(file->bytes.end(),in,in+n);return n;}
  bool seek(size_t n){if(n>size())return false;pos=n;return true;}
  bool available(){return file&&pos<size();}
  void flush(){}
  void close(){file.reset();}
  bool isDirectory(){return file&&file->directory;}
  String name(){return path;}
  File openNextFile(){size_t i=0;for(auto& entry:fs)if(entry.first.rfind(path+"/",0)==0){if(i++==index){index++;return File(entry.first,entry.second);}}return File();}
};
struct FakeSD {
  File open(const String& p,int mode=0){if(mode==FILE_WRITE)fs[p]=std::make_shared<FakeFile>();auto i=fs.find(p);return i==fs.end()?File():File(p,i->second);}
  bool exists(const String& p){return fs.count(p);}
  bool rename(const String& a,const String& b){if(!exists(a)||exists(b))return false;fs[b]=fs[a];fs.erase(a);return true;}
  uint64_t totalBytes(){return 8000000000ULL;}
  uint64_t usedBytes(){return 1000000;}
};
extern FakeSD SD_MMC;
namespace NIMBLE_PROPERTY {constexpr int READ=1,WRITE=2;}
class NimBLECharacteristic;
struct NimBLECharacteristicCallbacks {virtual ~NimBLECharacteristicCallbacks()=default;virtual void onWrite(NimBLECharacteristic*){}};
class NimBLECharacteristic {
  std::string value;
  std::unique_ptr<NimBLECharacteristicCallbacks> callbacks;
public:
  void setCallbacks(NimBLECharacteristicCallbacks* c){callbacks.reset(c);}
  void setValue(const char* s){value=s;}
  std::string getValue(){return value;}
  void write(const std::vector<uint8_t>& v){value.assign(v.begin(),v.end());callbacks->onWrite(this);}
};
struct NimBLEService {
  std::map<std::string,std::unique_ptr<NimBLECharacteristic>> chars;
  NimBLECharacteristic* createCharacteristic(const char* id,int){chars[id].reset(new NimBLECharacteristic());return chars[id].get();}
};
constexpr uint16_t TFT_WHITE=0xffff,TFT_BLACK=0,TFT_CYAN=0x7ff,TFT_GREEN=0x7e0,TFT_ORANGE=0xfd20,TFT_BLUE=31;
namespace fonts {constexpr int Font0=0,FreeSansBold9pt7b=1;}
struct FakeCanvas {
  std::vector<std::string> labels;
  int frames=0,lines=0,circles=0;
  void setTextColor(uint16_t,uint16_t){}
  void drawString(const String& s,int,int){labels.push_back(s);}
  void fillScreen(uint16_t){labels.clear();circles=0;}
  void setTextSize(int){}
  void setFont(const int*){}
  void setTextPadding(int){}
  void setClipRect(int,int,int,int){}
  void clearClipRect(){}
  void drawLine(int,int,int,int,uint16_t){lines++;}
  void drawFastHLine(int,int,int,uint16_t){}
  void drawWideLine(int,int,int,int,int,uint16_t){}
  void fillRoundRect(int,int,int,int,int,uint16_t){}
  void fillTriangle(int,int,int,int,int,int,uint16_t){}
  void fillCircle(int,int,int,uint16_t){circles++;}
  void pushSprite(int,int){frames++;}
};
extern FakeCanvas canvas;
extern int rasterLines,rasterFrames,mapBlitX,mapBlitY;
struct LGFX_Sprite : FakeCanvas {
  void setPsram(bool){}
  void setColorDepth(int){}
  void* createSprite(int,int){return failMemory?nullptr:this;}
  void deleteSprite(){}
  void setPivot(int,int){}
  void fillScreen(uint16_t c){FakeCanvas::fillScreen(c);lines=0;rasterFrames++;}
  void drawLine(int a,int b,int c,int d,uint16_t color){FakeCanvas::drawLine(a,b,c,d,color);rasterLines++;}
  void pushSprite(FakeCanvas* dst,int x,int y){dst->lines+=lines;mapBlitX=x;mapBlitY=y;}
  void pushRotateZoom(FakeCanvas* dst,float x,float y,float,float,float){dst->lines+=lines;mapBlitX=int(x);mapBlitY=int(y);}
};
