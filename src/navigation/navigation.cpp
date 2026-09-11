#include "navigation.h"
#include "map_renderer.h"
#include "ui/ride_menu.h"
#include "ui/control_layout.h"
#include "geo.h"
#include "storage/sd_access.h"
#include "hardware/display.h"
#include "hardware/power.h"
#include <SD_MMC.h>
#include <vector>
#include <algorithm>
#include <atomic>
#include <mutex>
#include "ps_buffer.h"

namespace {
// Navigation state is independent of SD I/O: loading a background tile must
// not prevent the UI from moving an already-rasterized map.
std::recursive_mutex stateMutex;
struct NavGuard {
  std::unique_lock<std::recursive_mutex> lock;
  bool locked;
  explicit NavGuard(bool wait=true):lock(stateMutex,std::defer_lock),
    locked(wait?(lock.lock(),true):lock.try_lock()){}
};
NimBLECharacteristic* control;
std::mutex replyMutex;
// GATT callbacks must never wait for navigation/SD locks or execute FAT I/O.
// One bounded mailbox matches the protocol's write -> read ACK flow.
std::mutex requestMutex;
std::string requestBytes;
bool requestData=false, requestPending=false, requestRunning=false;
std::atomic<bool> disconnectPending{false};
bool workerReady=false;
#ifndef UNIT_TEST
TaskHandle_t routeWorker=nullptr;
#endif
void wakeRouteWorker() {
#ifndef UNIT_TEST
  if(routeWorker)xTaskNotifyGive(routeWorker);
#endif
}
String workResult;
File incoming;
uint32_t expectedSize=0, received=0, expectedCrc=0, crc=0xffffffff, lastPacket=0;
std::atomic<bool> opened{false};
bool follow=true, choosing=false, cueList=false, located=false;
bool hasLiveLocation=false;
bool touchDown=false,touchMoved=false;
int downX=0,downY=0,previousX=0,previousY=0;
int zoom=15, listOffset=0;
double cx=0,cy=0;
PsBuffer<nav::Point> points, trail;
PsBuffer<nav::Cue> cues;
PsBuffer<double> cumulative;
struct Pixel {double x,y;};
PsBuffer<Pixel> projected;
String routeName, notice;
double progress=0, offDistance=0;
bool matched=false;
uint32_t lastDraw=0, lastTrack=0;
uint32_t lastMatch=0;
uint32_t word(const std::string& s, int i) { uint32_t v; memcpy(&v,s.data()+i,4); return v; }
void reply(const String& s) {
  std::lock_guard<std::mutex> lock(replyMutex);
  if(control)control->setValue(reinterpret_cast<const uint8_t*>(s.c_str()),s.length());
}
void resetTransfer() {
  if(incoming) {SdGuard sd;if(!sd.locked)return;incoming.close();}
  expectedSize=received=0;endRouteSync();
}
bool fileChecksum(File& f,uint32_t expected) {
  uint8_t block[512];uint32_t value=0xffffffff;size_t total=0;
  while(f.available()) {size_t n=f.read(block,sizeof(block));if(!n)return false;total+=n;value=nav::crc32(block,n,value);}
  bool ok=total==f.size() && (value^0xffffffff)==expected;f.seek(0);return ok;
}
bool validate(File& f) {
  nav::Header h;
  if(f.read((uint8_t*)&h,sizeof(h))!=sizeof(h) || !nav::headerValid(h,f.size())) return false;
  nav::Point p{},previous{};
  for(uint32_t i=0;i<h.points;i++) {
    if(f.read((uint8_t*)&p,sizeof(p))!=sizeof(p)||!nav::valid(p)) return false;
    // Regional packs do not wrap across the antimeridian.
    if(i && std::abs(double(p.lon)-previous.lon)>1800000000.0) return false;
    previous=p;
  }
  uint32_t last=0;
  for(uint32_t i=0;i<h.cues;i++) {
    nav::Cue c;
    if(f.read((uint8_t*)&c,sizeof(c))!=sizeof(c)||c.point>=h.points||c.point<last||c.text[42]!=0||c.direction < -2||c.direction>2) return false;
    last=c.point;
  }
  return true;
}
bool loadRoute(const String& path) {
  SdGuard sd;if(!sd.locked){notice="SD busy";return false;}
  File f=SD_MMC.open(path);
  if(!f || !validate(f)) { notice="invalid route"; return false; }
  f.seek(0); nav::Header h; f.read((uint8_t*)&h,sizeof(h));
  PsBuffer<nav::Point> nextPoints;
  PsBuffer<nav::Cue> nextCues;
  PsBuffer<double> nextDistances;
  PsBuffer<Pixel> nextProjected;
  if(!nextPoints.resize(h.points)||!nextCues.resize(h.cues)||!nextDistances.resize(h.points)||!nextProjected.resize(h.points)) {notice="PSRAM unavailable";return false;}
  if(f.read((uint8_t*)nextPoints.data(),h.points*sizeof(nav::Point))!=h.points*sizeof(nav::Point) ||
     (h.cues && f.read((uint8_t*)nextCues.data(),h.cues*sizeof(nav::Cue))!=h.cues*sizeof(nav::Cue))) {notice="SD read failed";return false;}
  for(size_t i=0;i<nextPoints.size();i++)nextProjected[i]={nav::x(nextPoints[i]),nav::y(nextPoints[i])};
  points.swap(nextPoints);cues.swap(nextCues);cumulative.swap(nextDistances);projected.swap(nextProjected);
  cumulative[0]=0;
  for(size_t i=1;i<points.size();i++) cumulative[i]=cumulative[i-1]+nav::distance(points[i-1],points[i]);
  routeName=h.name; progress=0; matched=false; choosing=false; follow=true;lastMatch=0;
  if(!hasLiveLocation) { cx=nav::x(points[0]);cy=nav::y(points[0]);located=true; }
  notice=""; return true;
}
std::vector<String> routeFiles() {
  std::vector<String> result;
  SdGuard sd;if(!sd.locked)return result;
  File dir=SD_MMC.open("/routes");
  if(!dir) return result;
  for(File f=dir.openNextFile();f && result.size()<64; f=dir.openNextFile()) {
    String name=f.name(); if(!f.isDirectory() && name.endsWith(".ocr")) { if(!name.startsWith("/")) name="/routes/"+name; result.push_back(name); }
  }
  std::sort(result.begin(),result.end()); return result;
}
class ControlWork {
  void reply(const String& s) {workResult=s;}
public:
  void process(const std::string& v) {
    NavGuard nav;
    SdGuard sd; if(!sd.locked) { reply("ERR busy");return; }
    if(v.empty()) return;
    if(!g_sd_ready) { reply("ERR no SD");return; }
    if(isPowerOffRequested()) { reply("ERR powering off");return; }
    if(v[0]==1 && v.size()==9) {
      resetTransfer(); expectedSize=word(v,1);expectedCrc=word(v,5);crc=0xffffffff;
      if(!beginRouteSync()) { expectedSize=0;reply("ERR update or shutdown busy");return; }
      if(expectedSize<sizeof(nav::Header)+16 || expectedSize>sizeof(nav::Header)+nav::maxPoints*8+nav::maxCues*48 || SD_MMC.totalBytes()-SD_MMC.usedBytes()<expectedSize+65536) { resetTransfer();reply("ERR size or space");return; }
      incoming=SD_MMC.open("/routes/incoming.part",FILE_WRITE);
      if(!incoming) { resetTransfer();reply("ERR write");return; }
      lastPacket=millis(); reply("OK 0");
    } else if(v[0]==2 && v.size()==1) {
      if(!incoming || received!=expectedSize || (crc^0xffffffff)!=expectedCrc) { resetTransfer();reply("ERR checksum or size");return; }
      incoming.flush();incoming.close();
      File f=SD_MMC.open("/routes/incoming.part"); bool good=f && fileChecksum(f,expectedCrc) && validate(f); f.close();
      char path[40]; snprintf(path,sizeof(path),"/routes/%08lx.ocr",(unsigned long)expectedCrc);
      resetTransfer();
      if(!good) { reply("ERR route");return; }
      if(SD_MMC.exists(path)) {
        File existing=SD_MMC.open(path), staged=SD_MMC.open("/routes/incoming.part");
        bool same=existing && staged && existing.size()==staged.size();
        uint8_t a[256],b[256];
        while(same && staged.available()) {size_t n=staged.read(b,256);same=n && existing.read(a,n)==n && !memcmp(a,b,n);}
        if(!same) {reply("ERR route id collision");return;}
      } else if(routeFiles().size()>=64) {reply("ERR route library full (64)");return;}
      if(!SD_MMC.exists(path) && !SD_MMC.rename("/routes/incoming.part",path)) { reply("ERR save");return; }
      reply(String("SAVED ")+path);
    } else if(v[0]==3) { resetTransfer();reply("OK cancelled");
    } else if(v[0]==4 && v.size()==5) {
      char path[40]; snprintf(path,sizeof(path),"/routes/%08lx.ocr",(unsigned long)word(v,1));
      reply(loadRoute(path)?String("OK selected"):String("ERR ")+notice); opened=true;
    } else if(v[0]==5) { points.clear();cues.clear();cumulative.clear();routeName="";opened=true;choosing=false;cueList=false;follow=true;reply("OK free ride");
    } else if(v[0]==6 && v.size()==5) {
      char path[40];snprintf(path,sizeof(path),"/routes/%08lx.ocr",(unsigned long)word(v,1));
      File route=SD_MMC.open(path),bounds=SD_MMC.open("/maps/coverage.bin");
      char magic[4];double b[4];nav::Header h;
      if(!route || !validate(route)) {reply("ERR route");return;}
      if(!bounds || bounds.read((uint8_t*)magic,4)!=4 || memcmp(magic,"OCB1",4) || bounds.read((uint8_t*)b,32)!=32) {reply("MAP unknown");return;}
      route.seek(0);route.read((uint8_t*)&h,sizeof(h));bool covered=true;
      int lastX=-1;bool column=false;
      for(uint32_t i=0;i<h.points;i++) {
        nav::Point p;if(route.read((uint8_t*)&p,sizeof(p))!=sizeof(p)) {covered=false;break;}
        double x=nav::x(p),y=nav::y(p);int tx=int(floor(x/256));
        if(tx!=lastX) {lastX=tx;snprintf(path,sizeof(path),"/maps/14/%d.ocp",tx);column=SD_MMC.exists(path);}
        if(x<b[0]||x>b[2]||y<b[1]||y>b[3]||!column)covered=false;
      }
      reply(covered?"MAP covered":"MAP missing");
    } else { reply("ERR command"); }
  }
};
class DataWork {
  void reply(const String& s) {workResult=s;}
public:
  void process(const std::string& v) {
    NavGuard nav;
    SdGuard sd; if(!sd.locked) {reply("ERR SD busy");return;}
    if(!incoming || v.size()<5 || word(v,0)!=received || v.size()-4>expectedSize-received || millis()-lastPacket>30000 || isPowerOffRequested()) { resetTransfer();reply("ERR offset or timeout");return; }
    size_t n=v.size()-4;
    if(incoming.write((const uint8_t*)v.data()+4,n)!=n) { resetTransfer();reply("ERR SD full");return; }
    crc=nav::crc32((const uint8_t*)v.data()+4,n,crc); received+=n; lastPacket=millis();reply(String("OK ")+String(received));
  }
};
void enqueue(NimBLECharacteristic* ch,bool data) {
  const std::string bytes=ch->getValue();
  std::lock_guard<std::mutex> lock(requestMutex);
  if(!workerReady) {reply("ERR route worker unavailable");return;}
  if(requestPending || requestRunning || disconnectPending.load()) {reply("ERR request pending");return;}
  if(bytes.empty() || bytes.size()>(data?484u:9u)) {reply("ERR packet size");return;}
  requestBytes=bytes;requestData=data;
  reply("BUSY");
  requestPending=true;
  wakeRouteWorker();
}
class Control : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* ch) override {enqueue(ch,false);}
};
class Data : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* ch) override {enqueue(ch,true);}
};
int sx(double x) { return int((x-cx)*std::pow(2,zoom-14)+120); }
int sy(double y) { return int((y-cy)*std::pow(2,zoom-14)+153); }
void label(const String& s,int x,int y,uint16_t color=TFT_WHITE) { canvas.setTextColor(color,TFT_BLACK);canvas.drawString(s,x,y); }
void match(nav::Point p) {
  if(points.size()<2) return;
  double best=1e30,candidate=progress,raw=1e30;
  for(size_t i=1;i<points.size();i++) {
    double f,d=nav::segmentDistance(p,points[i-1],points[i],f);
    double along=cumulative[i-1]+f*(cumulative[i]-cumulative[i-1]);
    // Limit reattachment along the track after the initial fix. Crossings must
    // not jump ahead to the end of a loop; reselect route to start elsewhere.
    if(matched && (along<progress-100 || along>progress+500)) continue;
    double score=d+(matched?std::abs(along-progress)*0.01:0);
    if(score<best) {best=score;raw=d;candidate=along;}
  }
  offDistance=raw;
  if(raw<=50) {progress=candidate;matched=true;}
}
}
void initNavigationService(NimBLEService* service) {
  control=service->createCharacteristic("00001904-0000-1000-8000-00805f9b34fb",NIMBLE_PROPERTY::READ|NIMBLE_PROPERTY::WRITE);
  control->setCallbacks(new Control());
  control->setValue(reinterpret_cast<const uint8_t*>("READY"),5);
  auto data=service->createCharacteristic("00001905-0000-1000-8000-00805f9b34fb",NIMBLE_PROPERTY::WRITE);
  data->setCallbacks(new Data());
#ifndef UNIT_TEST
  workerReady=xTaskCreate([](void*) {
    for(;;) {
      // Writes/disconnects wake immediately. The slow periodic wake only
      // checks transfer expiry and retries cleanup if the SD was busy.
      ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000));
      tickRouteTransfer();
    }
  },"RouteIO",12288,nullptr,1,&routeWorker)==pdPASS;
  Serial.printf("[ROUTE] SD worker %s (12 KB stack)\n",workerReady?"ready":"FAILED");
#else
  workerReady=true;
#endif
}
void abortRouteTransfer() { disconnectPending=true;wakeRouteWorker(); }
void detachNavigationService() {
  // The worker can finish cleanup after BLE shutdown, but must not touch a
  // characteristic that NimBLEDevice::deinit(true) is about to delete.
  std::lock_guard<std::mutex> requestLock(requestMutex);
  workerReady=false;disconnectPending=true;
  std::lock_guard<std::mutex> replyLock(replyMutex);
  control=nullptr;
  wakeRouteWorker();
}
void tickRouteTransfer() {
  // Only the RouteIO task (or the deterministic host test) calls this function.
  if(disconnectPending.load()) {
    NavGuard nav;
    resetTransfer();
    if(incoming)return; // SD busy: retry cleanup before accepting another request.
    std::lock_guard<std::mutex> lock(requestMutex);
    requestPending=false;requestBytes.clear();disconnectPending=false;
    reply("ERR disconnected");
    return;
  }
  std::string bytes;bool data=false;
  {
    std::lock_guard<std::mutex> lock(requestMutex);
    if(requestPending) {
      bytes.swap(requestBytes);data=requestData;
      requestPending=false;requestRunning=true;
    }
  }
  if(!bytes.empty()) {
    workResult="ERR command";
#ifndef UNIT_TEST
    const uint32_t started=millis();
#endif
    if(data)DataWork().process(bytes);else ControlWork().process(bytes);
#ifndef UNIT_TEST
    if(!data)Serial.printf("[ROUTE] cmd=%u ms=%lu stack_free=%u\n",unsigned(uint8_t(bytes[0])),
      (unsigned long)(millis()-started),unsigned(uxTaskGetStackHighWaterMark(nullptr)));
#endif
    // Publish the ACK only once the mailbox can accept the next packet.
    std::lock_guard<std::mutex> lock(requestMutex);
    requestRunning=false;
    reply(disconnectPending.load()?String("ERR disconnected"):workResult);
  }
  NavGuard nav(false);
  if(nav.locked && incoming && millis()-lastPacket>30000)resetTransfer();
}
bool navigationOpen() { return opened; }
void openNavigation() { NavGuard nav;opened=true;lastDraw=0; }
void navigationGesture(int x0,int y0,int x1,int y1) {
  NavGuard nav;
  int dx=x1-x0,dy=y1-y0;
  if(abs(dx)>12||abs(dy)>12) {
    if(y0>=44 && y0<255 && !choosing && !cueList) {follow=false;cx-=dx/std::pow(2,zoom-14);cy-=dy/std::pow(2,zoom-14);cx=std::max(0.0,std::min(nav::world,cx));cy=std::max(0.0,std::min(nav::world,cy));}
  } else if(y1<44) {
    if(ui::mapRide.contains(x1,y1)){openRideMenu();return;}
    if(ui::mapBack.contains(x1,y1)) {opened=false;return;}
    if(ui::mapRoutes.contains(x1,y1)){choosing=!choosing;cueList=false;listOffset=0;}
  } else if(y1>=266) {
    const int action=ui::mapControlHit(x1,y1,choosing||cueList);
    if(choosing||cueList) { if(action==0) listOffset=std::max(0,listOffset-4);else if(action==2) listOffset+=4;else if(action==1){choosing=false;cueList=false;} }
    else if(action==0) zoom=std::max(13,zoom-1);
    else if(action==1) zoom=std::min(17,zoom+1);
    else if(action==2) follow=true;
    else if(action==3){cueList=true;listOffset=0;}
  } else if(choosing) {
    if(y1<65) {points.clear();cues.clear();cumulative.clear();routeName="";choosing=false;}
    else if(y1<245) {auto files=routeFiles();int i=listOffset+(y1-65)/45;if(i>=0&&i<int(files.size())) loadRoute(files[i]);}
  }
  lastDraw=0;
}
void cancelNavigationTouch() { touchDown=false;touchMoved=false; }
void navigationTouch(bool touched,int x,int y) {
  // Never wait behind route sync to sample the next finger position. If busy,
  // retain the previous position so the next sample includes the full delta.
  NavGuard nav(false);if(!nav.locked)return;
  if(touched) {
    if(!touchDown) {
      touchDown=true;touchMoved=false;downX=previousX=x;downY=previousY=y;
      return;
    }
    if(!touchMoved && (abs(x-downX)>6 || abs(y-downY)>6))touchMoved=true;
    if(touchMoved) {
      if(downY>=44 && downY<250 && !choosing && !cueList) {
        double scale=std::pow(2,zoom-14);
        follow=false;cx=std::max(0.0,std::min(nav::world,cx-(x-previousX)/scale));
        cy=std::max(0.0,std::min(nav::world,cy-(y-previousY)/scale));
      }
      previousX=x;previousY=y;
    }
  } else if(touchDown) {
    touchDown=false;
    // A drag starting on a button must not become a tap on another button.
    if(!touchMoved)navigationGesture(downX,downY,downX,downY);
  }
}
void updateNavigation(const TelemetryState& state) {
  NavGuard nav(false);if(!nav.locked)return;
  bool fix=state.gps_has_fix && std::isfinite(state.lat) && std::isfinite(state.lon) && std::abs(state.lat)<=85 && std::abs(state.lon)<=180;
  static int lastRideState=RIDE_STATE_IDLE;
  if(state.ride_state==RIDE_STATE_ACTIVE && lastRideState==RIDE_STATE_IDLE)trail.clear();
  lastRideState=state.ride_state;
  if(!fix || state.ride_state!=RIDE_STATE_ACTIVE) {
    // Separate recorded sections; do not draw a fictitious line across GPS loss
    // or a pause. The complete GPX still belongs to the logger task.
    if(!trail.empty() && nav::valid(trail.back()) && trail.size()<512) {
      size_t n=trail.size();if(trail.resize(n+1))trail[n]={INT32_MAX,INT32_MAX};
    }
  }
  if(!fix)return;
  nav::Point here{int32_t(state.lat*1e7),int32_t(state.lon*1e7)};
  if(!lastMatch || millis()-lastMatch>=1000) {match(here);lastMatch=millis();}
  if(state.ride_state==RIDE_STATE_ACTIVE && millis()-lastTrack>=1000 &&
     (trail.empty()||!nav::valid(trail.back())||nav::distance(trail.back(),here)>3)) {
    lastTrack=millis();
    if(trail.size()==512) {memmove(trail.data(),trail.data()+1,511*sizeof(nav::Point));trail.resize(511);}
    size_t n=trail.size();if(trail.resize(n+1))trail[n]=here;
  }
}
void renderNavigation(const TelemetryState& state) {
  updateNavigation(state);
  NavGuard nav(false);if(!nav.locked)return;
  if(incoming && millis()-lastPacket>30000) resetTransfer();
  if(lastDraw && millis()-lastDraw<33)return;
  lastDraw=millis();
  bool fix=state.gps_has_fix && std::isfinite(state.lat) && std::isfinite(state.lon) && std::abs(state.lat)<=85 && std::abs(state.lon)<=180;
  nav::Point here{0,0};
  if(fix)here={int32_t(state.lat*1e7),int32_t(state.lon*1e7)};
  if(!fix && !located) {
    // Preview REAL SD map data around Monas before the first GPS fix. This is
    // only a viewport: never inject a synthetic fix into telemetry or logging.
    const nav::Point preview{-61754000,1068272000};
    cx=nav::x(preview);cy=nav::y(preview);located=true;
  }
  if(fix) {
    // First real fix always exits preview, even if the preview was panned.
    if(!hasLiveLocation)follow=true;
    if(follow||!located) {cx=nav::x(here);cy=nav::y(here);located=true;}
    hasLiveLocation=true;
  }
  canvas.fillScreen(TFT_BLACK);canvas.setTextSize(1);canvas.setFont(&fonts::Font0);canvas.setTextPadding(0);
  ui::drawIcon(canvas,ui::Icon::Back,10,10,TFT_WHITE);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setClipRect(48,0,144,44);
  label(choosing?"Routes":routeName.length()?routeName.substring(0,16):"Free ride",48,12,ui::accent);
  canvas.clearClipRect();
  ui::drawIcon(canvas,ui::Icon::Ride,206,10,ui::success);
  canvas.setFont(&fonts::Font0);
  if(choosing) {
    label("free ride",10,45,ui::success);
    auto files=routeFiles();listOffset=std::min(listOffset,std::max(0,int(files.size())-1));
    for(int i=0;i<4 && listOffset+i<int(files.size());i++) {
      SdGuard sd;if(!sd.locked)break;
      File f=SD_MMC.open(files[listOffset+i]);nav::Header h{};f.read((uint8_t*)&h,sizeof(h));h.name[47]=0;
      label(String(h.name).substring(0,32),10,76+i*45);
    }
    if(files.empty())label(g_sd_ready?"sync GPX from the app":"SD unavailable",10,110,ui::warning);
    if(notice.length())label(notice,10,253,ui::warning);
  } else if(cueList) {
    listOffset=std::min(listOffset,std::max(0,int(cues.size())-1));
    for(int i=0;i<4 && listOffset+i<int(cues.size());i++) {
      auto& c=cues[listOffset+i];label(String(cumulative[c.point]/1000,1)+" km",10,50+i*48,ui::accent);label(String(c.text).substring(0,35),10,65+i*48);
    }
    if(cues.empty()) label("no route instructions",10,60);
  } else {
    MapStatus mapStatus=MapStatus::Loading;
    canvas.setClipRect(0,44,240,206);
    if(located && g_sd_ready) {
      mapStatus=drawMapBackground(cx,cy,zoom);
    }
    auto line=[&](const PsBuffer<nav::Point>& ps,uint16_t color) {
      for(size_t i=1;i<ps.size();i++) {
        if(!nav::valid(ps[i-1])||!nav::valid(ps[i]))continue;
        bool route=&ps==&points;
        double ax=route?projected[i-1].x:nav::x(ps[i-1]),ay=route?projected[i-1].y:nav::y(ps[i-1]);
        double bx=route?projected[i].x:nav::x(ps[i]),by=route?projected[i].y:nav::y(ps[i]);
        double margin=256/std::pow(2,zoom-14);
        if(std::max(ax,bx)<cx-margin||std::min(ax,bx)>cx+margin||std::max(ay,by)<cy-margin||std::min(ay,by)>cy+margin)continue;
        canvas.drawLine(sx(ax),sy(ay),sx(bx),sy(by),color);
        if(route)canvas.drawLine(sx(ax)+1,sy(ay),sx(bx)+1,sy(by),color);
      }
    };
    line(trail,ui::success);line(points,ui::accent);
    if(fix) {int x=sx(nav::x(here)),y=sy(nav::y(here));canvas.fillCircle(x,y,5,TFT_WHITE);canvas.fillCircle(x,y,2,TFT_BLUE);}
    canvas.clearClipRect();
    if(!g_sd_ready)label("SD unavailable",4,45,ui::warning);
    else if(mapStatus==MapStatus::Loading)label("loading map...",4,45,ui::warning);
    else if(mapStatus==MapStatus::Missing)label("map missing: copy area to SD",4,45,ui::warning);
    else if(mapStatus==MapStatus::NoMemory)label("map cache: PSRAM unavailable",4,45,ui::warning);
    label("N ^  z"+String(zoom)+(follow?" follow":" pan"),4,235);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    if(!fix) label(state.gps_fix_quality==1?(hasLiveLocation?"GPS weak / held":"Preview / GPS weak"):hasLiveLocation?"GPS lost":"Preview / no GPS",4,244,ui::warning);
    else if(points.empty()) label("Free ride",4,244,ui::success);
    else if(offDistance>50)label("Off route / rejoin GPX",4,244,ui::warning);
    else if(matched && cumulative.back()-progress<20)label("Arrived / GPX end",4,244,ui::success);
    else {
      String text=String((cumulative.back()-progress)/1000,1)+" km left";
      for(auto& c:cues) if(cumulative[c.point]>progress+8) {
        text=String(int(cumulative[c.point]-progress))+"m "+c.text;
        if(c.direction) {
          // High-contrast upcoming-turn arrow, independent of map orientation.
          canvas.fillRoundRect(199,38,36,38,4,TFT_BLACK);
          canvas.drawWideLine(217,69,217,51,3,ui::accent);
          int tip=c.direction<0?204:230;
          canvas.drawWideLine(217,51,tip,51,3,ui::accent);
          canvas.fillTriangle(tip,51,tip+(c.direction<0?7:-7),45,tip+(c.direction<0?7:-7),57,ui::accent);
        }
        break;
      }
      // Important guidance is readable; extra cue detail remains in the list.
      canvas.setClipRect(0,244,240,20);
      label(text.substring(0,39),4,244,ui::accent);
      canvas.clearClipRect();
    }
  }
  canvas.setFont(&fonts::Font0);
  const bool list=choosing||cueList;
  const ui::Icon mapIcons[]={ui::Icon::Minus,ui::Icon::Plus,ui::Icon::Center,ui::Icon::List};
  const ui::Icon listIcons[]={ui::Icon::Back,ui::Icon::Map,ui::Icon::Next};
  const char* listLabels[]={"Prev","Map","Next"};
  for(int i=0;i<(list?3:4);i++) {
    const auto& r=list?ui::listControls[i]:ui::mapControls[i];
    const uint16_t bg=!list&&i==2&&follow?0x0230:ui::panel;
    canvas.fillRoundRect(r.x+2,r.y,r.w-4,r.h,8,bg);
    ui::drawIcon(canvas,list?listIcons[i]:mapIcons[i],list?r.x+7:r.x+(r.w-24)/2,r.y+10,ui::accent);
    if(list){canvas.setTextColor(TFT_WHITE,bg);canvas.drawString(listLabels[i],r.x+36,r.y+18);}
  }
  label("(c) OpenStreetMap contributors",4,312,0x8410);
  canvas.pushSprite(0,0);
}
