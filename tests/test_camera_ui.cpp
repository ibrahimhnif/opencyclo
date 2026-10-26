#include "widget_fakes.h"
#include "ui/engine/widget_registry.h"
#include "ui/control_layout.h"
#include "hardware/camera_protocol.h"
#include <cassert>
Canvas canvas,tft;
FakeSerial Serial;
Settings g_settings;
bool g_ble_scanning=false,pairing=false,subscribed=false;
bool label(const char* value){for(const auto& s:canvas.labels)if(s==value)return true;return false;}
int main(int argc,char**) {
  const TemplateSlot slot{{4,44,232,258},SIZE_FULL};
  auto tap=[&](ui::HitRect r){assert(handleWidgetTouch(WIDGET_CAMERA_REMOTE,slot,slot.rect.x+r.x+r.w/2,slot.rect.y+r.y+r.h/2));};

  // Capture page: Pair scans for the camera, Shutter is the direct-link
  // shutter, Options is UI-only.
  tap(ui::captureRect(0,232));tap(ui::captureRect(1,232));tap(ui::captureRect(2,232));
  assert(cameraPairCalls()==1&&cameraShutterCalls()==1&&cameraPowerOffCalls()==0);

  // Options page: Modes is UI-only, Off powers the camera off over the direct
  // link; the two lower slots are empty (no Screen/Wake without the remote).
  tap(ui::optionRect(1,232));
  assert(cameraPowerOffCalls()==1);
  assert(!handleWidgetTouch(WIDGET_CAMERA_REMOTE,slot,slot.rect.x+ui::optionRect(2,232).x+4,slot.rect.y+ui::optionRect(2,232).y+4));
  assert(!handleWidgetTouch(WIDGET_CAMERA_REMOTE,slot,slot.rect.x+ui::optionRect(3,232).x+4,slot.rect.y+ui::optionRect(3,232).y+4));
  assert(cameraPowerOffCalls()==1&&cameraShutterCalls()==1);

  // Options -> Modes opens the paged picker; tapping a mode sends setCameraMode.
  tap(ui::optionRect(0,232));
  tap(ui::optionRect(0,232));
  assert(cameraModeCalls()==1&&lastCameraSubMode()==camera_proto::CAMERA_MODES[0].subMode);
  tap(ui::optionRect(3,232));
  assert(cameraModeCalls()==2&&lastCameraSubMode()==camera_proto::CAMERA_MODES[3].subMode);
  tap(ui::modeNav(1,232));            // next page
  tap(ui::optionRect(0,232));         // first mode of page 2
  assert(cameraModeCalls()==3&&lastCameraSubMode()==camera_proto::CAMERA_MODES[4].subMode);
  tap(ui::modeNav(0,232));            // back to options
  tap(ui::captureRect(0,232));        // back to capture

  assert(!handleWidgetTouch(WIDGET_CAMERA_REMOTE,slot,14,55));  // status
  assert(!handleWidgetTouch(WIDGET_CAMERA_REMOTE,slot,14,280)); // hint
  const TemplateSlot invalid{{4,28,232,274},SIZE_SMALL};
  assert(!handleWidgetTouch(WIDGET_CAMERA_REMOTE,invalid,30,130));

  const char* names[]={"camera_idle","camera_pairing","camera_connected"};
  for(int i=0;i<3;i++){
    pairing=i==1;subscribed=i==2;
    canvas.fillScreen(0);
    renderWidget(WIDGET_CAMERA_REMOTE,slot,TelemetryState{},true);
    assert(label("Shutter")&&label("Options")&&!label("Modes"));
    assert(!label("Recording"));
    assert(label(pairing?"Camera pairing":subscribed?"Connected":"Not connected"));
    assert(!label("Waking (10s)")&&!label("Pairing (30s)"));
    if(argc>1){printf("SCREEN|%s\n",names[i]);for(auto& c:canvas.commands)puts(c.c_str());}
    assert(!label("54%")&&label("Shutter follows mode"));  // no status reported yet
  }
  // Status indicator: what the camera reported over the direct link -- battery
  // (top right), free storage and the recording timer (bottom row).
  {
    camera_proto::CameraStatus& s=fakeCameraStatus();
    s.battery_pct=54;s.charging=true;s.card_state=0;s.free_bytes=13335003136ULL;
    s.capture_state=1;s.capture_secs=65;s.photo_sub=100;s.video_sub=5;
    pairing=false;subscribed=true;
    canvas.fillScreen(0);
    renderWidget(WIDGET_CAMERA_REMOTE,slot,TelemetryState{},true);
    assert(label("Connected")&&label("54%")&&label("SD 12.4G")&&label("REC 1:05"));
    assert(!label("Shutter follows mode"));
    if(argc>1){puts("SCREEN|camera_status");for(auto& c:canvas.commands)puts(c.c_str());}
    s.capture_state=0;
    canvas.fillScreen(0);
    renderWidget(WIDGET_CAMERA_REMOTE,slot,TelemetryState{},true);
    assert(label("Freeframe")&&!label("REC 1:05"));
    // Dropping the link hides the indicator again.
    subscribed=false;
    canvas.fillScreen(0);
    renderWidget(WIDGET_CAMERA_REMOTE,slot,TelemetryState{},true);
    assert(!label("54%")&&!label("SD 12.4G")&&label("Shutter follows mode"));
    s=camera_proto::CameraStatus();
  }
  // Options page render.
  tap(ui::captureRect(2,232));canvas.fillScreen(0);
  renderWidget(WIDGET_CAMERA_REMOTE,slot,TelemetryState{},true);
  assert(label("Modes")&&label("Off")&&label("Back")&&!label("Screen")&&!label("Wake"));
  if(argc>1)puts("SCREEN|camera_options"),(void)0;
  // Modes page render. The picker was left on its second page above, so cycle
  // all the way round back to the first before asserting labels.
  tap(ui::optionRect(0,232));
  const int pages=((int)camera_proto::CAMERA_MODE_COUNT+3)/4;
  for(int i=0;i<pages-1;i++)tap(ui::modeNav(1,232));
  canvas.fillScreen(0);
  renderWidget(WIDGET_CAMERA_REMOTE,slot,TelemetryState{},true);
  assert(label("Video")&&label("Freeframe")&&label("Next")&&label("Back"));
  assert(label(camera_proto::CAMERA_MODES[0].label)&&label(camera_proto::CAMERA_MODES[1].label));
  if(argc==1)puts("Production camera UI: direct-link shutter/off, paged mode picker, invalid slots and busy states passed");
  return 0;
}
