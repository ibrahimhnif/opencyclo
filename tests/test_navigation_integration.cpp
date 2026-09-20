#include "navigation_fakes.h"
#include "navigation/navigation.h"
#include "navigation/geo.h"
#include "navigation/map_renderer.h"
#include <cassert>

uint32_t fakeMillis=1;
bool failMemory=false,syncOwned=false,updating=false,g_sd_ready=true;
bool fakeSdBusy=false;
std::map<std::string,std::shared_ptr<FakeFile>> fs;
FakeSD SD_MMC;
FakeCanvas canvas;
int rideOpens=0;
void openRideMenu(){rideOpens++;}
int rasterLines=0,rasterFrames=0,mapBlitX=0,mapBlitY=0;
bool beginRouteSync(){if(syncOwned||updating)return false;return syncOwned=true;}
void endRouteSync(){syncOwned=false;}
bool isPowerOffRequested(){return false;}
void put(std::vector<uint8_t>& b,uint32_t n){for(int i=0;i<4;i++)b.push_back((n>>(i*8))&255);}
bool label(const char* s){for(auto& text:canvas.labels)if(text.find(s)!=std::string::npos)return true;return false;}

void previewMapFixture() {
  nav::Point center{-61754000,1068272000};
  int tx=int(nav::x(center)/256),ty=int(nav::y(center)/256);
  auto file=std::make_shared<FakeFile>();
  file->bytes={'O','C','P','1'};put(file->bytes,1);put(file->bytes,ty);put(file->bytes,20);put(file->bytes,1);
  uint16_t coords[]={0,0,65535,65535};auto raw=(const uint8_t*)coords;
  file->bytes.insert(file->bytes.end(),raw,raw+8);file->bytes.push_back(1);
  fs["/maps/14/"+std::to_string(tx)+".ocp"]=file;
  auto bounds=std::make_shared<FakeFile>();bounds->bytes={'O','C','B','1'};
  double box[]={nav::x(center)-1000,nav::y(center)-1000,nav::x(center)+1000,nav::y(center)+1000};
  raw=(const uint8_t*)box;bounds->bytes.insert(bounds->bytes.end(),raw,raw+sizeof(box));fs["/maps/coverage.bin"]=bounds;
}

const char* g_fakeLastScreenshot=nullptr;
int main(int argc,char**){
  if(argc>1) {
    failMemory=true;
    assert(drawMapBackground(0,0,15)==MapStatus::NoMemory);
    puts("Map raster allocation failure handled");return 0;
  }
  fprintf(stderr,"navigation: transfer checks\n");
  fs["/routes"]=std::make_shared<FakeFile>();fs["/routes"]->directory=true;
  NimBLEService service;initNavigationService(&service);
  auto control=service.chars["00001904-0000-1000-8000-00805f9b34fb"].get();
  auto data=service.chars["00001905-0000-1000-8000-00805f9b34fb"].get();
  assert(control->getValue()=="READY"); // Text bytes only, no pointer or trailing NUL.
  fs["/rides"]=std::make_shared<FakeFile>();fs["/rides"]->directory=true;
  fs["/rides/test.gpx"]=std::make_shared<FakeFile>();fs["/rides/test.gpx"]->bytes={'a','b','c'};
  control->write({0x10,0,0,0,0});assert(control->getValue()=="FILE 3 test.gpx");
  control->write({0x10,1,0,0,0});assert(control->getValue()=="END");
  std::vector<uint8_t> download{0x11,0,0,0,0};std::string file="test.gpx";
  download.insert(download.end(),file.begin(),file.end());
  control->writeOnly(download);assert(control->getValue()=="BUSY");
  tickRouteTransfer();assert(control->getValue()=="DATA 0 352441c2 616263");
  download[1]=3;control->write(download);assert(control->getValue().find("ERR")==0);download[1]=0;
  exportTestState().ride_state=RIDE_STATE_ACTIVE;
  control->write(download);assert(control->getValue().find("ERR finish")==0);
  exportTestState().ride_state=RIDE_STATE_IDLE;exportTestState().ride_save=RIDE_SAVE_PENDING;
  control->write(download);assert(control->getValue().find("ERR finish")==0);
  exportTestState().ride_save=RIDE_SAVE_NONE;
  download={0x11,0,0,0,0};file="../test.gpx";download.insert(download.end(),file.begin(),file.end());
  control->write(download);assert(control->getValue()=="ERR filename");
  previewMapFixture();
  auto fastData=service.chars["00001906-0000-1000-8000-00805f9b34fb"].get();
  control->write({0x15});assert(control->getValue()=="CAPS 64");
  std::vector<uint8_t> fastOpen{0x12,3,0,0,0};file="test.gpx";
  fastOpen.insert(fastOpen.end(),file.begin(),file.end());
  control->write(fastOpen);assert(control->getValue()=="FAST 1 3");
  control->write({0x13,1,0,0,0,0,0,0,0,12,0});
  assert(control->getValue()=="BLOCK 0 3 352441c2");
  assert(fastData->notifications.size()==1);
  assert(fastData->notifications[0].size()==11 && fastData->notifications[0].substr(8)=="abc");
  control->write({0x13,2,0,0,0,0,0,0,0,12,0});assert(control->getValue()=="ERR export session");
  control->write({0x13,1,0,0,0,3,0,0,0,12,0});assert(control->getValue()=="ERR export offset");
  control->write({0x14,1,0,0,0});assert(control->getValue()=="OK closed");
  control->write(fastOpen);assert(control->getValue()=="FAST 2 3");
  fakeMillis+=31001;tickRouteTransfer();
  control->write({0x13,2,0,0,0,0,0,0,0,12,0});assert(control->getValue()=="ERR export session");
  auto& big=fs["/rides/test.gpx"]->bytes;big.resize(8001);
  for(size_t i=0;i<big.size();i++)big[i]=uint8_t(i);
  fastOpen[1]=0x41;fastOpen[2]=0x1f; // 8001 bytes
  control->write(fastOpen);assert(control->getValue()=="FAST 3 8001");
  fastData->notifications.clear();
  control->write({0x13,3,0,0,0,0,0,0,0,0xe0,1}); // 480-byte payload
  assert(control->getValue()=="ERR export MTU 180");
  assert(fastData->notifications.empty()); // Reject before allocating any packets.
  control->write({0x13,3,0,0,0,0,0,0,0,180,0});
  assert(fastData->notifications.size()==16);
  for(size_t i=0;i<16;i++){
    const auto& packet=fastData->notifications[i];assert(packet.size()==188);
    uint32_t offset;memcpy(&offset,packet.data()+4,4);assert(offset==i*180);
    assert(!memcmp(packet.data()+8,big.data()+offset,180));
  }
  fastData->notifications.clear();
  control->write({0x13,3,0,0,0,0xf0,0x1e,0,0,180,0}); // final 81 bytes
  assert(fastData->notifications.size()==1 && fastData->notifications[0].size()==89);
  control->write({0x13,3,0,0,0,0,0,0,0,180,0}); // explicit rewind/retry
  assert(control->getValue().find("BLOCK 0 2880 ")==0);
  fastData->notifications.clear();
  control->write({0x13,3,0,0,0,0,0,0,0,180,0,64});
  assert(control->getValue().find("BLOCK 0 8001 ")==0);
  assert(fastData->notifications.size()==45);
  control->write({0x13,3,0,0,0,0,0,0,0,180,0,65});
  assert(control->getValue()=="ERR export credits");
  abortRouteTransfer();tickRouteTransfer();
  control->write({0x13,3,0,0,0,0,0,0,0,12,0});assert(control->getValue()=="ERR export session");
  TelemetryState waiting;
  renderNavigation(waiting);
  assert(label("Preview / no GPS") && canvas.lines>0 && canvas.circles==0);
  assert(!waiting.gps_has_fix && waiting.lat==0 && waiting.lon==0);
  const int cachedFrames=rasterFrames,cachedLines=rasterLines,startX=mapBlitX;
  fakeMillis+=40;renderNavigation(waiting);
  assert(rasterFrames==cachedFrames && rasterLines==cachedLines);
  navigationTouch(true,100,100);
  navigationTouch(true,120,100);
  fakeMillis+=40;renderNavigation(waiting);
  assert(label(" pan") && mapBlitX==startX+20 && rasterFrames==cachedFrames);
  navigationTouch(true,140,100);
  fakeMillis+=40;renderNavigation(waiting);
  assert(mapBlitX==startX+40 && rasterFrames==cachedFrames);
  navigationTouch(false,0,0); // No duplicate pan on release.
  fakeMillis+=40;renderNavigation(waiting);
  assert(mapBlitX==startX+40);
  navigationTouch(true,100,100);
  fakeSdBusy=true;navigationTouch(true,120,100);
  fakeMillis+=40;renderNavigation(waiting);
  assert(mapBlitX==startX+60 && rasterFrames==cachedFrames);
  fakeSdBusy=false;navigationTouch(true,130,100);
  fakeMillis+=40;renderNavigation(waiting);
  assert(mapBlitX==startX+70 && rasterFrames==cachedFrames);
  navigationTouch(false,0,0);
  // Cache refresh threshold and diagonal movement, still before release.
  navigationTouch(true,100,100);navigationTouch(true,130,120);
  fakeMillis+=40;renderNavigation(waiting);
  assert(rasterFrames==cachedFrames+1);
  navigationTouch(false,0,0);
  // Two quick taps must both count (no former 150 ms suppression).
  navigationTouch(true,70,284);navigationTouch(false,0,0);
  fakeMillis+=40;renderNavigation(waiting);assert(label("z16"));
  navigationTouch(true,70,284);navigationTouch(false,0,0);
  fakeMillis+=40;renderNavigation(waiting);assert(label("z17"));
  navigationTouch(true,70,284);navigationTouch(false,0,0);
  fakeMillis+=40;renderNavigation(waiting);assert(label("z17"));
  // Dragging off a button must not activate zoom.
  navigationTouch(true,20,284);navigationTouch(true,70,284);navigationTouch(false,0,0);
  fakeMillis+=40;renderNavigation(waiting);assert(label("z17"));
  navigationTouch(true,20,284);cancelNavigationTouch();navigationTouch(false,0,0);
  fakeMillis+=40;renderNavigation(waiting);assert(label("z17"));
  for(int i=0;i<2;i++){navigationTouch(true,20,284);navigationTouch(false,0,0);}
  fakeMillis+=40;renderNavigation(waiting);assert(label("z15"));
  // Icon-only controls keep the same actions, while attribution is inert.
  navigationGesture(70,314,70,314);
  fakeMillis+=40;renderNavigation(waiting);assert(label("z15"));
  navigationGesture(130,284,130,284);
  fakeMillis+=40;renderNavigation(waiting);assert(label("follow"));
  navigationGesture(200,284,200,284);
  fakeMillis+=40;renderNavigation(waiting);assert(label("no route instructions"));
  navigationGesture(120,284,120,284);
  fakeMillis+=40;renderNavigation(waiting);assert(label("z15"));
  navigationGesture(90,20,90,20);
  fakeMillis+=40;renderNavigation(waiting);assert(label("Routes"));
  navigationGesture(120,284,120,284);
  navigationGesture(217,38,217,38);assert(rideOpens==1);
  openNavigation();navigationGesture(20,38,20,38);assert(!navigationOpen());
  openNavigation();
  fakeMillis+=1000;
  nav::Header h{};memcpy(h.magic,"OCR1",4);h.points=3;h.cues=1;strcpy(h.name,"test ride");
  nav::Point points[]={{0,0},{10000,0},{10000,10000}};
  nav::Cue cue{};cue.point=1;cue.direction=1;strcpy(cue.text,"route bends right");
  std::vector<uint8_t> bytes(sizeof(h)+sizeof(points)+sizeof(cue));
  memcpy(bytes.data(),&h,sizeof(h));memcpy(bytes.data()+sizeof(h),points,sizeof(points));memcpy(bytes.data()+sizeof(h)+sizeof(points),&cue,sizeof(cue));
  uint32_t checksum=nav::crc32(bytes.data(),bytes.size())^0xffffffff;
  std::vector<uint8_t> begin{1};put(begin,bytes.size());put(begin,checksum);
  // No SD access or transfer ownership until the worker runs.
  control->writeOnly(begin);
  assert(control->getValue()=="BUSY" && !syncOwned && !fs.count("/routes/incoming.part"));
  abortRouteTransfer(); // Disconnect cancels queued work without doing SD I/O.
  assert(!syncOwned);
  tickRouteTransfer();assert(control->getValue()=="ERR disconnected" && !syncOwned);
  control->writeOnly(begin);
  control->writeOnly({5});assert(control->getValue()=="ERR request pending");
  tickRouteTransfer();assert(control->getValue()=="OK 0" && syncOwned);
  abortRouteTransfer();assert(syncOwned);
  fakeSdBusy=true;tickRouteTransfer();assert(syncOwned);
  fakeSdBusy=false;tickRouteTransfer();assert(!syncOwned);
  data->write(std::vector<uint8_t>(485,0));assert(control->getValue()=="ERR packet size");
  control->write(begin);assert(control->getValue()=="OK 0" && syncOwned);
  std::vector<uint8_t> wrong;put(wrong,1);wrong.push_back(0);data->write(wrong);
  assert(control->getValue().find("ERR")==0 && !syncOwned);
  auto upload=[&](){
    control->write(begin);
    for(size_t offset=0;offset<bytes.size();){size_t end=std::min(offset+16,bytes.size());std::vector<uint8_t> chunk;put(chunk,offset);chunk.insert(chunk.end(),bytes.begin()+offset,bytes.begin()+end);data->write(chunk);assert(control->getValue()=="OK "+std::to_string(end));offset=end;}
    control->write({2});assert(control->getValue().find("SAVED ")==0 && !syncOwned);
  };
  upload();upload(); // Re-sync is idempotent and does not overwrite another file.
  fprintf(stderr,"navigation: load and render checks\n");
  std::vector<uint8_t> select{4};put(select,checksum);
  failMemory=true;control->write(select);assert(control->getValue()=="ERR PSRAM unavailable");
  failMemory=false;control->write(select);assert(control->getValue()=="OK selected");assert(navigationOpen());
  TelemetryState state;state.gps_has_fix=true;state.ride_state=RIDE_STATE_ACTIVE;
  renderNavigation(state);assert(label("route bends right") && label("follow"));
  assert(!label("preview") && canvas.circles>0);
  fprintf(stderr,"navigation: GPS and timeout checks\n");
  fakeMillis+=1100;state.lat=0.0001;state.lon=0.01;renderNavigation(state);assert(label("Off route"));
  fakeMillis+=1100;state.gps_has_fix=false;renderNavigation(state);assert(label("GPS lost"));
  fakeMillis+=1100;state.gps_fix_quality=1;renderNavigation(state);assert(label("GPS weak / held"));
  control->write({5});fakeMillis+=1000;state.gps_has_fix=true;renderNavigation(state);assert(label("Free ride"));
  state.speed_kmh=20;fakeMillis+=1000;renderNavigation(state);
  state.lat+=0.0001;fakeMillis+=2000;renderNavigation(state);
  assert(canvas.triangles==3 && canvas.circles==0);
  state.speed_kmh=0;fakeMillis+=1000;renderNavigation(state);
  assert(canvas.triangles==3 && canvas.circles==0); // held heading
  state.gps_has_fix=false;fakeMillis+=1000;renderNavigation(state);
  assert(canvas.triangles==0 && canvas.circles==0); // no phantom live marker
  control->write(begin);fakeMillis+=31000;tickRouteTransfer();assert(!syncOwned);
  updating=true;control->write(begin);assert(control->getValue().find("ERR")==0);updating=false;
  g_sd_ready=false;control->write(begin);assert(control->getValue()=="ERR no SD");g_sd_ready=true;
  control->write(begin);abortRouteTransfer();tickRouteTransfer();assert(!syncOwned);
  // Read an indexed map column through the production renderer.
  nav::Point center{10000000,10000000};int tx=int(nav::x(center)/256),ty=int(nav::y(center)/256);
  std::string mapPath="/maps/14/"+std::to_string(tx)+".ocp";
  auto column=std::make_shared<FakeFile>();
  column->bytes={'O','C','P','1'};put(column->bytes,1);put(column->bytes,ty);put(column->bytes,20);put(column->bytes,1);
  uint16_t coords[]={0,0,65535,65535};
  auto raw=(const uint8_t*)coords;column->bytes.insert(column->bytes.end(),raw,raw+8);column->bytes.push_back(1);fs[mapPath]=column;
  auto bounds=std::make_shared<FakeFile>();bounds->bytes={'O','C','B','1'};
  double box[]={nav::x(center)-1000,nav::y(center)-1000,nav::x(center)+1000,nav::y(center)+1000};
  raw=(const uint8_t*)box;bounds->bytes.insert(bounds->bytes.end(),raw,raw+sizeof(box));fs["/maps/coverage.bin"]=bounds;
  state.lat=1;state.lon=1;fakeMillis+=1000;int before=canvas.lines;
  renderNavigation(state);assert(canvas.lines>before);
  // Maximum overscan footprint (5 x 5 tiles) and world-edge coordinates.
  drawMapBackground(512,512,13);
  assert(drawMapBackground(0,0,13)==MapStatus::Missing);
  assert(drawMapBackground(nav::world,nav::world,13)==MapStatus::Missing);
  // Last-screenshot download: 0x16 info, 0x17 opens the same fast-export session from /screenshots.
  exportTestState().ride_state=RIDE_STATE_IDLE;exportTestState().ride_save=RIDE_SAVE_NONE;
  control->write({0x16});assert(control->getValue()=="ERR no screenshot");
  fs["/screenshots/20260920_083012.bmp"]=std::make_shared<FakeFile>();
  fs["/screenshots/20260920_083012.bmp"]->bytes={'x','y','z'};
  g_fakeLastScreenshot="/screenshots/20260920_083012.bmp";
  control->write({0x16});assert(control->getValue()=="SHOT 3 20260920_083012.bmp");
  std::vector<uint8_t> shotOpen{0x17,3,0,0,0};std::string shotName="20260920_083012.bmp";
  shotOpen.insert(shotOpen.end(),shotName.begin(),shotName.end());
  control->write(shotOpen);
  const std::string opened=control->getValue();assert(opened.rfind("FAST ",0)==0);
  assert(opened.substr(opened.find(' ',5)+1)=="3");
  const uint32_t shotToken=uint32_t(std::stoul(opened.substr(5)));
  const uint8_t t0=uint8_t(shotToken),t1=uint8_t(shotToken>>8),t2=uint8_t(shotToken>>16),t3=uint8_t(shotToken>>24);
  fastData->notifications.clear();
  control->write({0x13,t0,t1,t2,t3,0,0,0,0,12,0});
  assert(control->getValue().rfind("BLOCK 0 3 ",0)==0);
  assert(fastData->notifications.size()==1 && fastData->notifications[0].substr(8)=="xyz");
  control->write({0x14,t0,t1,t2,t3});assert(control->getValue()=="OK closed");
  // Ride names and traversal are rejected by the screenshot opener too.
  shotOpen={0x17,3,0,0,0};shotName="test.gpx";shotOpen.insert(shotOpen.end(),shotName.begin(),shotName.end());
  control->write(shotOpen);assert(control->getValue()=="ERR filename");
  shotOpen={0x17,3,0,0,0};shotName="../20260920_083012.bmp";shotOpen.insert(shotOpen.end(),shotName.begin(),shotName.end());
  control->write(shotOpen);assert(control->getValue()=="ERR filename");
  control->writeOnly(begin);
  detachNavigationService();service.chars.clear();
  tickRouteTransfer(); // Cleanup must not access characteristics deleted by BLE.
  assert(!syncOwned);
  puts("Navigation transfer, validation, memory failure, rendering and GPS loss tests passed");
}
