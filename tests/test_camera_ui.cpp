#include "widget_fakes.h"
#include "ui/engine/widget_registry.h"
#include "ui/camera_view.h"
#include "hardware/camera_protocol.h"
#include <cassert>
Canvas canvas,tft;
FakeSerial Serial;
Settings g_settings;
bool g_ble_scanning=false,pairing=false,subscribed=false;
static const TemplateSlot slot{{4,44,232,258},SIZE_FULL};
static bool label(const char* value){for(const auto& s:canvas.labels)if(s==value)return true;return false;}
static bool tap(ui::HitRect r){
  return handleWidgetTouch(WIDGET_CAMERA_REMOTE,slot,slot.rect.x+r.x+r.w/2,slot.rect.y+r.y+r.h/2);}
static int screens=0;
static void render(const char* name,bool print){
  canvas.fillScreen(0);
  renderWidget(WIDGET_CAMERA_REMOTE,slot,TelemetryState{},true);
  if(print){printf("SCREEN|%s\n",name);for(auto& c:canvas.commands)puts(c.c_str());}
  ++screens;
}
static void addCandidate(const char* name,const char* mac,int8_t rssi){
  camera_proto::CameraCandidate& c=fakeCandidates()[fakeCandidateCount()++];
  snprintf(c.name,sizeof(c.name),"%s",name);snprintf(c.mac,sizeof(c.mac),"%s",mac);c.addr_type=1;c.rssi=rssi;
}

int main(int argc,char**) {
  const bool print=argc>1;
  const int w=232;

  // ---- Scan: nothing saved, nothing found yet --------------------------------
  render("camera_scan_empty",print);
  assert(label("Not connected")&&label("Scan")&&!label("Forget"));
  assert(label("Turn the camera on,")&&label("then tap Scan"));
  assert(tap(camui::topButton(0,1,w))&&cameraScanCalls()==1);
  assert(!tap(camui::scanRow(0,w)));  // no rows to tap

  // ---- Scan: two cameras found, strongest first; tap connects to that one -----
  pairing=true;
  render("camera_scanning",print);
  assert(label("Scanning...")&&label("Looking for")&&label("cameras..."));
  addCandidate("Ace Pro 2 4Y2KD9","aa:bb:cc:00:00:01",-82);
  addCandidate("Ace Pro 2 7QX1A3","aa:bb:cc:00:00:02",-48);
  render("camera_scan_list",print);
  assert(label("Scanning...")&&label("Scanning"));
  assert(label("Ace Pro 2 7QX1A3")&&label("-48 dBm")&&label("Ace Pro 2 4Y2KD9")&&label("-82 dBm"));
  assert(label("Tap a camera to connect"));
  assert(tap(camui::scanRow(0,w))&&cameraConnectCalls()==1);
  assert(!strcmp(lastCameraConnect().mac,"aa:bb:cc:00:00:02"));
  pairing=false;fakeCameraConnecting()=true;
  render("camera_connecting",print);
  assert(label("Connecting..."));
  fakeCameraConnecting()=false;

  // A saved camera adds Forget next to Scan.
  fakeCameraSaved()=true;fakeCandidateCount()=0;
  render("camera_scan_saved",print);
  assert(label("Forget")&&label("Reconnecting to the")&&label("saved camera..."));
  assert(tap(camui::topButton(1,2,w))&&cameraForgetCalls()==1);

  // ---- Connected: lands on Modes, with battery + storage -------------------
  subscribed=true;
  camera_proto::CameraStatus& s=fakeCameraStatus();
  s.battery_pct=54;s.charging=false;s.card_state=0;s.free_bytes=13335003136ULL;
  s.capture_state=0;s.photo_sub=100;s.video_sub=0;
  render("camera_modes",print);
  assert(label("Connected")&&label("54%")&&label("12.4G free")&&label("More")&&label("Next"));
  assert(label("Video")&&label("Free")&&label("1/4"));
  assert(tap(camui::topButton(1,2,w)));  // next page
  render("camera_modes_p2",print);
  assert(label("2/4")&&!label("Video"));
  for(int p=3;p<=camui::modePages();p++){  // remaining pages, so every mode icon is previewed
    assert(tap(camui::topButton(1,2,w)));
    char name[24];snprintf(name,sizeof(name),"camera_modes_p%d",p);
    render(name,print);
  }
  tap(camui::topButton(1,2,w));  // wraps back round to page 1
  render("camera_modes_back",print);
  assert(label("1/4"));

  // ---- Pick Freeframe -> capture page (video) --------------------------------
  assert(tap(camui::modeCell(1,w)));
  assert(cameraModeCalls()==1&&lastCameraScope()==camera_proto::SCOPE_VIDEO&&lastCameraSubMode()==5);
  render("camera_video_ready",print);
  assert(label("Modes")&&label("VIDEO")&&label("Freeframe")&&label("Ready")&&label("Record"));
  assert(label("Tap Record to start")&&!label("Stop"));
  assert(tap(camui::actionRect(0,1,w))&&cameraCommandCalls()[0]==1);

  // Record tapped: Starting... until the camera answers, Stop already offered.
  s.capture_request=camera_proto::CAPTURE_STARTING;
  render("camera_video_starting",print);
  assert(label("Starting...")&&label("Stop")&&!label("Record"));

  // Recording: REC timer + Stop only (no pause).
  s.capture_request=camera_proto::CAPTURE_NONE;s.capture_state=1;s.capture_secs=65;
  render("camera_video_rec",print);
  assert(label("REC 1:05")&&label("Stop")&&!label("Record")&&!label("Pause"));
  assert(tap(camui::actionRect(0,1,w))&&cameraCommandCalls()[1]==1);  // stop

  // Stop tapped: Saving... with no button while the camera writes the file.
  s.capture_request=camera_proto::CAPTURE_STOPPING;
  render("camera_video_stopping",print);
  assert(label("Saving...")&&!label("Stop")&&!label("Record"));
  assert(!tap(camui::actionRect(0,1,w)));

  s.capture_request=camera_proto::CAPTURE_NONE;s.capture_state=0;s.capture_secs=0;
  snprintf(s.last_file,sizeof(s.last_file),"VID_20261026_110301_255.mp4");
  render("camera_video_saved",print);
  assert(label("Ready")&&label("Saved VID_20261026_110301_255.mp4")&&label("Record"));

  // ---- Back to Modes, then to the Photo mode ----------------------------------
  assert(tap(camui::topButton(0,2,w)));
  render("camera_modes_again",print);
  assert(label("More"));
  int photoIndex=-1;
  for(size_t i=0;i<camera_proto::CAMERA_MODE_COUNT;i++)
    if(!strcmp(camera_proto::CAMERA_MODES[i].label,"Photo"))photoIndex=int(i);
  assert(photoIndex>=0);
  for(int p=0;p<photoIndex/camui::kModeCells;p++)tap(camui::topButton(1,2,w));
  assert(tap(camui::modeCell(photoIndex%camui::kModeCells,w)));
  assert(lastCameraScope()==camera_proto::SCOPE_PHOTO&&lastCameraSubMode()==0);
  s.last_file[0]='\0';s.photo_sub=0;s.video_sub=100;
  render("camera_photo_ready",print);
  assert(label("PHOTO")&&label("Photo")&&label("Take photo")&&label("Ready"));
  assert(tap(camui::actionRect(0,1,w))&&cameraCommandCalls()[3]==1);
  s.photo_state=camera_proto::PHOTO_PENDING;
  render("camera_photo_saving",print);
  assert(label("Saving..."));
  s.photo_state=camera_proto::PHOTO_SAVED;snprintf(s.last_file,sizeof(s.last_file),"IMG_20261026_110241_254.jpg");
  render("camera_photo_saved",print);
  assert(label("Saved")&&label("IMG_20261026_110241_254.jpg")&&label("12.4G free"));

  // ---- More: Off and Forget ---------------------------------------------------
  assert(tap(camui::topButton(0,2,w)));       // capture -> modes
  assert(tap(camui::topButton(0,2,w)));       // modes -> more
  render("camera_more",print);
  assert(label("Back")&&label("Off")&&label("Forget"));
  assert(tap(camui::modeCell(0,w))&&cameraCommandCalls()[4]==1);
  assert(tap(camui::topButton(0,2,w)));       // back to modes
  render("camera_modes_from_more",print);
  assert(label("More"));

  // ---- Link drops: back to Scan, whatever page was open ------------------------
  subscribed=false;
  render("camera_dropped",print);
  assert(label("Not connected")&&label("Scan")&&!label("More"));

  // Invalid slot sizes stay inert.
  const TemplateSlot invalid{{4,28,232,274},SIZE_SMALL};
  assert(!handleWidgetTouch(WIDGET_CAMERA_REMOTE,invalid,30,130));
  if(!print)printf("Production camera UI: scan list, connect, modes, video/photo capture, more, link drop (%d screens) passed\n",screens);
  return 0;
}
