#include "widget_fakes.h"
#include "ui/engine/widget_registry.h"
#include "ui/control_layout.h"
#include <cassert>
Canvas canvas,tft;
FakeSerial Serial;
Settings g_settings;
bool g_ble_scanning=false,pairing=false,waking=false,subscribed=false,wakeReady=false;
int cameraCalls[6]{};
bool label(const char* value){for(const auto& s:canvas.labels)if(s==value)return true;return false;}
int main(int argc,char**) {
  const TemplateSlot slot{{4,44,232,258},SIZE_FULL};
  auto tap=[&](ui::HitRect r){assert(handleWidgetTouch(WIDGET_CAMERA_REMOTE,slot,slot.rect.x+r.x+r.w/2,slot.rect.y+r.y+r.h/2));};
  tap(ui::captureRect(0,232));tap(ui::captureRect(1,232));
  tap(ui::captureRect(2,232)); // UI-only Options, sends no command.
  for(int i=0;i<4;i++)tap(ui::optionRect(i,232));
  for(int i=0;i<6;i++)assert(cameraCalls[i]==1);
  tap(ui::captureRect(0,232)); // Back to Capture, sends no command.
  assert(!handleWidgetTouch(WIDGET_CAMERA_REMOTE,slot,14,55)); // status
  assert(!handleWidgetTouch(WIDGET_CAMERA_REMOTE,slot,14,280)); // hint
  const TemplateSlot invalid{{4,28,232,274},SIZE_SMALL};
  assert(!handleWidgetTouch(WIDGET_CAMERA_REMOTE,invalid,30,130));
  const char* names[]={"camera_idle","camera_pairing","camera_connected","camera_waking","camera_ready"};
  for(int i=0;i<5;i++){
    pairing=i==1;subscribed=i>=2;waking=i==3;wakeReady=i>=3;
    canvas.fillScreen(0);
    renderWidget(WIDGET_CAMERA_REMOTE,slot,TelemetryState{},true);
    assert(label("Shutter")&&label("Options")&&!label("Mode"));
    assert(!label("Recording"));
    assert(label(pairing?"Pairing (30s)":waking?"Waking (10s)":subscribed?"Connected":"Not connected"));
    if(argc>1){printf("SCREEN|%s\n",names[i]);for(auto& c:canvas.commands)puts(c.c_str());}
  }
  tap(ui::captureRect(2,232));canvas.fillScreen(0);
  renderWidget(WIDGET_CAMERA_REMOTE,slot,TelemetryState{},true);
  assert(label("Mode")&&label("Screen")&&label("Wake")&&label("Off")&&label("Back"));
  if(argc>1){puts("SCREEN|camera_options");for(auto& c:canvas.commands)puts(c.c_str());}
  if(argc==1)puts("Production camera UI: all six original command dispatches, invalid slots and busy states passed");
}
