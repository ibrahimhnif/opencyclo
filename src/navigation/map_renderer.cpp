#include "map_renderer.h"
#include "geo.h"
#include "ocn_index.h"
#include "ps_buffer.h"
#include "hardware/display.h"
#include "hardware/power.h"
#include "storage/sd_access.h"
#include <SD_MMC.h>
#include <algorithm>
#include <cstring>
#ifndef UNIT_TEST
#include <freertos/task.h>
#endif

namespace {
constexpr int size=480;
struct Tile { int x=-1,y=-1; bool present=false; PsBuffer<uint8_t> bytes; };
Tile tiles[16];
size_t nextTile=0; // size_t: a signed counter would wrap (UB) and index negatively
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

struct NamedTile { int x=-1,y=-1; bool present=false; PsBuffer<uint8_t> pool; PsBuffer<uint8_t> segments; };
NamedTile namedTiles[4];
size_t nextNamedTile=0;
NamedTile& namedTile(int x,int y) {
  for(auto& t:namedTiles) if(t.x==x&&t.y==y) return t;
  NamedTile& t=namedTiles[nextNamedTile++%4];t.x=x;t.y=y;t.pool.clear();t.segments.clear();t.present=false;
  char path[64];snprintf(path,sizeof(path),"/maps/14/%d.ocn",x);
  File f=SD_MMC.open(path);
  if(!f)return t; // No named-road data for this column -- not an error.
  uint8_t h[12];
  if(f.read(h,12)!=12 || memcmp(h,"OCN1",4))return t;
  uint32_t indexCount,poolSize;memcpy(&indexCount,h+4,4);memcpy(&poolSize,h+8,4);
  if(indexCount>16384 || poolSize>65535 || f.size()<12+uint64_t(indexCount)*12+poolSize)return t;
  // One sequential read of the whole index table (<=196,608 B, PSRAM) instead
  // of a seek+read per binary-search probe: fewer SD seeks, and the search
  // itself becomes the pure, unit-tested ocn::findRow().
  PsBuffer<uint8_t> index;
  if(indexCount && (!index.resize(indexCount*12) || !f.seek(12) ||
     f.read(index.data(),index.size())!=index.size()))return t;
  uint32_t offset=0,count=0;
  if(!ocn::findRow(index.data(),indexCount,(uint32_t)y,&offset,&count)) {
    t.present=true; // Empty row inside a completed pack -- no named roads here.
    return t;
  }
  if(count>20000 || 12+uint64_t(indexCount)*12+poolSize+uint64_t(offset)+count*10>f.size())return t;
  if(!t.pool.resize(poolSize) || !t.segments.resize(count*10))return t;
  if(!f.seek(12+indexCount*12) || (poolSize && f.read(t.pool.data(),poolSize)!=poolSize))return t;
  if(!f.seek(12+indexCount*12+poolSize+offset) || (count && f.read(t.segments.data(),t.segments.size())!=t.segments.size()))return t;
  t.present=true;
  return t;
}

struct View { double x=0,y=0; int zoom=15; };
// covered[] holds one flag per tile in the overscan block. 5x5 is the worst
// case for the supported zoom range (block span is 1.875 * 2^(14-zoom) tiles:
// 3.75 at the 13 floor, so at most 5 columns/rows). rasterize() re-clamps
// columns/rows to these bounds at the point where the array is actually
// indexed, so a future caller passing an unsupported zoom cannot overrun it.
constexpr int kMinZoom=13,kMaxZoom=17;
constexpr int kMaxOverscanColumns=5,kMaxOverscanRows=5;
struct Raster {
  LGFX_Sprite image;
  View view;
  bool valid=false;
  int left=0,top=0,columns=0,rows=0;
  bool covered[kMaxOverscanColumns*kMaxOverscanRows]{};
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
  out.columns=std::min(right-out.left+1,kMaxOverscanColumns);
  out.rows=std::min(bottom-out.top+1,kMaxOverscanRows);
  for(int y=out.top;y<out.top+out.rows;y++)for(int x=out.left;x<out.left+out.columns;x++) {
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
      const uint8_t style=t.bytes[i+8];
      const float baseWidth=style==1?2.0f:style==0?1.5f:1.0f;
      // Only ever thin out when zoomed out (s<1, e.g. 0.5 at zoom 13) --
      // never thicken above baseWidth, since the app's resting/default zoom
      // (15) already has s=2.0, and scaling up from there made every normal
      // view render roads twice as thick as intended. Floored at 1px so
      // roads don't disappear entirely at the most zoomed-out level.
      const float width=std::max(1.0f,baseWidth*std::min(1.0f,float(s)));
      const uint16_t color=style==2?0x35ad:(style==1?0x8410:0x4208);
      out.image.drawWideLine(ax,ay,bx,by,width,color);
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
  // Visible map area is screen rows ui::headerHeight(44)..249 with the view's
  // world anchor drawn at y=153 (drawMapBackground's +153 blit) and x=120, so
  // the exact world span is y-109/s..y+96/s and x-120/s..x+119/s. The top used
  // to be 121/s, a leftover from the 32px header: 12/s world px too far up, so
  // standing near the northern edge of a pack reported "map missing" while
  // every visible pixel was covered.
  int left=int(std::floor((v.x-120/s)/256)),right=int(std::floor((v.x+119/s)/256));
  int top=int(std::floor((v.y-109/s)/256)),bottom=int(std::floor((v.y+96/s)/256));
  for(int y=top;y<=bottom;y++)for(int x=left;x<=right;x++) {
    if(x<r.left || x>=r.left+r.columns || y<r.top || y>=r.top+r.rows ||
       !r.covered[(y-r.top)*r.columns+x-r.left])return false;
  }
  return true;
}
}
bool nearestRoadName(double lat,double lon,char* name,size_t nameLen) {
  if(nameLen==0)return false;
  nav::Point here{int32_t(lat*1e7),int32_t(lon*1e7)};
  if(!nav::valid(here))return false;
  double wx=nav::x(here),wy=nav::y(here);
  int tx=int(std::floor(wx/256)),ty=int(std::floor(wy/256));
  NamedTile* loaded=nullptr;
  {
    SdGuard sd;
    if(sd.locked && g_sd_ready && !isPowerOffRequested())loaded=&namedTile(tx,ty);
  }
  if(!loaded || !loaded->present || loaded->segments.size()==0)return false;
  // Cheap integer reject before any soft-float transcendental work. Segment
  // coordinates are tile-local 1/256-pixel units; at zoom 14 one such unit is
  // 0.03728/cos(lat) m on the ground, so 40 m is 1073/cos(lat) units. 1300 at
  // the equator (48.5 m) is deliberately loose so the box is a guaranteed
  // superset of what the exact segmentDistance check below accepts; the
  // 1/cos(lat) term keeps that true away from the equator too. One cos per
  // call replaces up to 20,000 unproject/segmentDistance evaluations.
  const double cosLat=std::cos(lat*nav::pi/180);
  const int reach=cosLat>0.05?int(1300.0/cosLat)+1:65535;
  const int hereLocalX=int((wx-tx*256.0)*256.0),hereLocalY=int((wy-ty*256.0)*256.0);
  double best=1e18;uint16_t bestOffset=0;bool found=false;
  for(size_t i=0;i<loaded->segments.size();i+=10) {
    uint16_t p[4];uint16_t nameOffset;
    memcpy(p,loaded->segments.data()+i,8);
    const int minX=std::min(p[0],p[2]),maxX=std::max(p[0],p[2]);
    const int minY=std::min(p[1],p[3]),maxY=std::max(p[1],p[3]);
    if(hereLocalX<minX-reach || hereLocalX>maxX+reach ||
       hereLocalY<minY-reach || hereLocalY>maxY+reach)continue;
    memcpy(&nameOffset,loaded->segments.data()+i+8,2);
    nav::Point a=nav::unproject(tx*256.0+p[0]/256.0,ty*256.0+p[1]/256.0);
    nav::Point b=nav::unproject(tx*256.0+p[2]/256.0,ty*256.0+p[3]/256.0);
    double fraction;double d=nav::segmentDistance(here,a,b,fraction);
    if(d<best && nameOffset<loaded->pool.size()) {best=d;bestOffset=nameOffset;found=true;}
  }
  if(!found || best>40)return false;
  const char* poolStr=(const char*)loaded->pool.data()+bestOffset;
  size_t maxLen=loaded->pool.size()-bestOffset;
  size_t len=strnlen(poolStr,maxLen);
  if(len>=nameLen)len=nameLen-1;
  memcpy(name,poolStr,len);name[len]=0;
  return true;
}
MapStatus drawMapBackground(double x,double y,int zoom) {
  initMapRenderer();if(failed)return MapStatus::NoMemory;
  View v;v.x=x;v.y=y;v.zoom=std::max(kMinZoom,std::min(kMaxZoom,zoom));
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
