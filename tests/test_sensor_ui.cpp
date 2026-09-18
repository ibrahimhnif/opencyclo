#include "widget_fakes.h"
#include "ui/engine/widget_registry.h"
#include "ui/scroll_gesture.h"
#include <cassert>
Canvas canvas,tft;
FakeSerial Serial;
Settings g_settings;
bool g_ble_scanning=false,pairing=false,waking=false,subscribed=false,wakeReady=false;
int cameraCalls[6]{};
bool label(const char* text){for(const auto& s:canvas.labels)if(s==text)return true;return false;}
int main(int argc,char**){
  const TemplateSlot slot{{4,44,232,258},SIZE_FULL};int swipe=0;
  TelemetryState state{};
  auto draw=[&](){canvas.fillScreen(0);renderWidget(WIDGET_BLE_MANAGER,slot,state,true);};
  auto preview=[&](const char* name){if(argc>1){printf("SCREEN|%s\n",name);for(auto& command:canvas.commands)puts(command.c_str());}};
  auto touch=[&](bool down,int x,int y){return handleSensorWidgetStream(slot.rect,down,x,y,swipe);};
  draw();assert(label("No paired sensors. Tap Scan.") && !label("Power") && !label("Forget"));
  preview("sensors_empty");
  fakeSensors().foundCount=8;
  for(int i=0;i<8;i++){auto& r=fakeSensors().found[i];snprintf(r.mac,18,"00:00:00:00:00:%02d",i);r.kind=SensorKind::Csc;}
  touch(true,180,65);touch(false,180,65);draw();assert(label("Back") && label("Connect"));
  preview("sensors_scan");
  touch(true,180,132); // First visible Connect row; freeze its identity.
  snprintf(fakeSensors().found[0].mac,18,"99:99:99:99:99:99");draw();
  touch(false,180,132);assert(sensorConnectCalls()==1);
  assert(!strcmp(lastSensorConnect().mac,"00:00:00:00:00:00"));
  draw();touch(true,180,190);touch(true,180,120);touch(false,180,120);
  assert(sensorConnectCalls()==1); // scroll never connects
  draw();preview("sensors_scrolled");
  draw();touch(true,180,140);touch(true,210,160);touch(true,180,140);touch(false,180,140);
  assert(sensorConnectCalls()==1); // drag out and back is not a tap
  touch(true,170,140);touch(true,100,140);touch(false,100,140);assert(swipe==1);
  touch(true,180,140);cancelSensorWidgetTouch();assert(!touch(false,180,140));
  fakeSensors().pairedCount=2;
  for(int i=0;i<2;i++){auto& r=fakeSensors().paired[i];snprintf(r.mac,18,"00:00:00:00:00:%02d",i);r.kind=i?SensorKind::Cadence:SensorKind::Speed;r.connected=true;}
  touch(true,30,65);touch(false,30,65);draw();preview("sensors_paired");
  assert(label("Forget") && label("Speed") && label("Cadence") && !label("Power"));
  touch(true,180,288);touch(false,180,288);draw();
  assert(label("Calibrate") && label("Waiting for barometer"));
  touch(true,120,260);touch(false,120,260);assert(fakeCalibrationElevation()==10000);
  state.baro_valid=true;state.baro_age_ms=0;draw();
  for(int i=0;i<5;i++){touch(true,140,150);touch(false,140,150);}
  draw();assert(label("5 m"));preview("baro_calibrate");
  touch(true,120,260);touch(false,120,260);assert(fakeCalibrationElevation()==5);
  state.ride_state=RIDE_STATE_ACTIVE;draw();assert(label("Finish ride to calibrate"));
  fakeCalibrationElevation()=10000;
  touch(true,120,260);touch(false,120,260);assert(fakeCalibrationElevation()==10000);
  touch(true,30,65);touch(false,30,65);draw();assert(label("Paired"));
  puts("Sensor UI: paired-only empty state, scan list, frozen selection, scroll/tap and horizontal paging passed");
}
