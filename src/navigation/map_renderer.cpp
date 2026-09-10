#include "map_renderer.h"
#include "geo.h"
#include "ps_buffer.h"
#include "hardware/display.h"
#include "hardware/power.h"
#include "storage/sd_access.h"
#include <SD_MMC.h>
#include <algorithm>
#ifndef UNIT_TEST
#include <freertos/task.h>
#endif

namespace {
constexpr int size=480;
struct Tile { int x=-1,y=-1; bool present=false; PsBuffer<uint8_t> bytes; };
Tile tiles[16];
int nextTile=0;
Tile& tile(int x,int y) {
  for(auto& t:tiles) if(t.x==x&&t.y==y) return t;
  Tile& t=tiles[nextTile++%16];t.x=x;t.y=y;t.bytes.clear();t.present=false;
  char path[64];snprintf(path,sizeof(path),"/maps/14/%d/%d.ocm",x,y);
  File f;if(SD_MMC.exists(path))f=SD_MMC.open(path);
  uint8_t h[8];
  if(!f) {
    snprintf(path,sizeof(path),"/maps/14/%d.ocp",x);
    f=SD_MMC.open(path);
    File bounds=SD_MMC.open("/maps/coverage.bin");char magic[4];double b[4];
    bool inside=bounds && bounds.read((uint8_t*)magic,4)==4 && !memcmp(magic,"OCB1",4) && bounds.read((uint8_t*)b,32)==32 &&
      x*256.0>=b[0]-256 && x*256.0<=b[2] && y*256.0>=b[1]-256 && y*256.0<=b[3];
    if(!inside || !f || f.read(h,8)!=8 || memcmp(h,"OCP1",4))return t;
    uint32_t count;memcpy(&count,h+4,4);
    if(count>16384 || f.size()<8+count*12)return t;
    uint32_t lo=0,hi=count;
    while(lo<hi) {
      uint32_t mid=(lo+hi)/2,entry[3];
      if(!f.seek(8+mid*12)||f.read((uint8_t*)entry,12)!=12)return t;
      if(entry[0]<(uint32_t)y)lo=mid+1;
      else if(entry[0]>(uint32_t)y)hi=mid;
      else {
        if(entry[2]>20000 || entry[1]<8+count*12 || uint64_t(entry[1])+entry[2]*9>f.size() || !t.bytes.resize(entry[2]*9))return t;
        t.present=f.seek(entry[1]) && (!entry[2]||f.read(t.bytes.data(),t.bytes.size())==t.bytes.size());return t;
      }
    }
    t.present=true; // Empty tile inside a completed map's coverage.
    return t;
  }
  if(f && f.read(h,8)==8 && !memcmp(h,"OCM1",4)) {
    uint32_t n;memcpy(&n,h+4,4);
    if(n<=20000 && f.size()==8+n*9 && t.bytes.resize(n*9)) { t.present=!n || f.read(t.bytes.data(),t.bytes.size())==t.bytes.size(); }
  }
  return t;
}

struct View { double x=0,y=0; int zoom=15; };
struct Raster {
  LGFX_Sprite image;
  View view;
  bool valid=false;
  int left=0,top=0,columns=0,rows=0;
  bool covered[25]{};
};
Raster buffers[2];
int front=0;
View requested,working;
bool pending=false,busy=false,initialized=false,failed=false;
#ifndef UNIT_TEST
SemaphoreHandle_t mutex=nullptr;
TaskHandle_t worker=nullptr;
struct Lock {
  Lock(){xSemaphoreTake(mutex,portMAX_DELAY);}
  ~Lock(){xSemaphoreGive(mutex);}
};
#else
struct Lock { Lock(){} ~Lock(){} };
#endif
double scale(const View& v){return std::pow(2.0,v.zoom-14);}
bool near(const View& a,const View& b,int margin) {
  return a.zoom==b.zoom && std::abs(a.x-b.x)*scale(a)<=margin && std::abs(a.y-b.y)*scale(a)<=margin;
}
void rasterize(Raster& out,const View& v) {
  out.view=v;out.image.fillScreen(TFT_BLACK);
  double s=scale(v);
  out.left=int(std::floor((v.x-size/2/s)/256));
  out.top=int(std::floor((v.y-size/2/s)/256));
  int right=int(std::floor((v.x+size/2/s)/256)),bottom=int(std::floor((v.y+size/2/s)/256));
  out.columns=right-out.left+1;out.rows=bottom-out.top+1;
  for(int y=out.top;y<=bottom;y++)for(int x=out.left;x<=right;x++) {
    Tile* loaded=nullptr;
    {
      // The worker alone owns tiles; release SD before the expensive raster loop.
      SdGuard sd;
      if(sd.locked && g_sd_ready && !isPowerOffRequested())loaded=&tile(x,y);
    }
    out.covered[(y-out.top)*out.columns+x-out.left]=loaded && loaded->present;
    if(!loaded || !loaded->present)continue;
    auto& t=*loaded;
    for(size_t i=0;i<t.bytes.size();i+=9) {
      uint16_t p[4];memcpy(p,t.bytes.data()+i,8);
      int ax=int((x*256.0+p[0]/256.0-v.x)*s+size/2);
      int ay=int((y*256.0+p[1]/256.0-v.y)*s+size/2);
      int bx=int((x*256.0+p[2]/256.0-v.x)*s+size/2);
      int by=int((y*256.0+p[3]/256.0-v.y)*s+size/2);
      if(std::max(ax,bx)<0 || std::min(ax,bx)>=size || std::max(ay,by)<0 || std::min(ay,by)>=size)continue;
      out.image.drawLine(ax,ay,bx,by,t.bytes[i+8]==2?0x35ad:(t.bytes[i+8]==1?0x8410:0x4208));
    }
#ifndef UNIT_TEST
    // Let core-0 services run even in dense cities.
    vTaskDelay(1);
#endif
  }
  out.valid=true;
}
bool step() {
  int back;View v;
  {
    Lock lock;
    if(!pending)return false;
    v=requested;working=v;pending=false;busy=true;back=1-front;
  }
  rasterize(buffers[back],v);
  {
    Lock lock;
    front=back;busy=false;
  }
  return true;
}
#ifndef UNIT_TEST
void run(void*) {
  for(;;) {ulTaskNotifyTake(pdTRUE,portMAX_DELAY);while(step()){}}
}
#endif
void initMapRenderer() {
  if(initialized)return;
  initialized=true;
  for(auto& b:buffers) {
    b.image.setPsram(true);b.image.setColorDepth(16);
    if(!b.image.createSprite(size,size)){failed=true;break;}
  }
#ifndef UNIT_TEST
  if(!failed) {
    mutex=xSemaphoreCreateMutex();
    if(!mutex || xTaskCreatePinnedToCore(run,"MapRaster",6144,nullptr,1,&worker,0)!=pdPASS)failed=true;
  }
#endif
  if(failed)for(auto& b:buffers)b.image.deleteSprite();
}
bool covered(const Raster& r,const View& v) {
  double s=scale(v);
  int left=int(std::floor((v.x-120/s)/256)),right=int(std::floor((v.x+119/s)/256));
  int top=int(std::floor((v.y-121/s)/256)),bottom=int(std::floor((v.y+96/s)/256));
  for(int y=top;y<=bottom;y++)for(int x=left;x<=right;x++) {
    if(x<r.left || x>=r.left+r.columns || y<r.top || y>=r.top+r.rows ||
       !r.covered[(y-r.top)*r.columns+x-r.left])return false;
  }
  return true;
}
}
MapStatus drawMapBackground(double x,double y,int zoom) {
  initMapRenderer();if(failed)return MapStatus::NoMemory;
  View v;v.x=x;v.y=y;v.zoom=zoom;
  {
    Lock lock;
    if(!buffers[front].valid || !near(v,buffers[front].view,80)) {
      if(!busy || !near(v,working,80)) {requested=v;pending=true;}
      else pending=false;
    } else pending=false;
  }
#ifdef UNIT_TEST
  step();
#else
  xTaskNotifyGive(worker);
#endif
  Lock lock;
  auto& r=buffers[front];
  if(!r.valid)return MapStatus::Loading;
  double s=scale(v),ratio=s/scale(r.view);
  int dx=int(std::round((r.view.x-x)*s+120)),dy=int(std::round((r.view.y-y)*s+153));
  if(r.view.zoom==zoom)r.image.pushSprite(&canvas,dx-size/2,dy-size/2);
  else {
    r.image.setPivot(size/2,size/2);
    r.image.pushRotateZoom(&canvas,dx,dy,0,ratio,ratio);
  }
  // Overscan may be exhausted during a fast drag; don't call that missing data.
  if(!near(v,r.view,118))return MapStatus::Loading;
  return covered(r,v)?MapStatus::Ready:MapStatus::Missing;
}
