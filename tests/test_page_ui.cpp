#include "widget_fakes.h"
#include "ui/engine/layout_manager.h"
#include "ui/control_layout.h"
#include <cassert>
Canvas canvas,tft;FakeSerial Serial;Settings g_settings;
bool g_ble_scanning=false,pairing=false,waking=false,subscribed=true,wakeReady=true;
int cameraCalls[6]{},mapOpens=0,rideOpens=0;
void openNavigation(){mapOpens++;}void openRideMenu(){rideOpens++;}
int main(int argc,char**){
  PageConfig pages[]={
    {"Ride",TEMPLATE_HERO_6_GRID,6,{WIDGET_SPEED,WIDGET_DISTANCE,WIDGET_RIDE_TIME,WIDGET_CADENCE,WIDGET_HEART_RATE,WIDGET_POWER}},
    {"Climb",TEMPLATE_2_GRID_CHART,3,{WIDGET_ALTITUDE,WIDGET_GRADE,WIDGET_ELEVATION_CHART}},
    {"Sensors",TEMPLATE_FULL_CONTAINER,1,{WIDGET_BLE_MANAGER}},
    {"Settings",TEMPLATE_FULL_CONTAINER,1,{WIDGET_SETTINGS_LIST}},
    {"Insta360",TEMPLATE_FULL_CONTAINER,1,{WIDGET_CAMERA_REMOTE}},
    {"4-grid",TEMPLATE_4_GRID,4,{WIDGET_DISTANCE,WIDGET_RIDE_TIME,WIDGET_AVG_SPEED,WIDGET_MAX_SPEED}},
    {"8-grid",TEMPLATE_8_GRID,8,{WIDGET_CADENCE,WIDGET_HEART_RATE,WIDGET_POWER,WIDGET_ALTITUDE,WIDGET_GRADE,WIDGET_TOTAL_ASCENT,WIDGET_BATTERY,WIDGET_AVG_SPEED}}
  };
  TelemetryState state{};state.gps_has_fix=true;state.ride_state=RIDE_STATE_ACTIVE;
  state.speed_kmh=24.6;state.trip_distance_km=32.75;state.ride_time_s=4360;
  state.cadence_rpm=88;state.heart_rate_bpm=146;state.power_watts=212;
  state.battery_pct=82;state.satellites=12;state.avg_speed_kmh=21.4;state.max_speed_kmh=48.2;
  const char* names[]={"dashboard","climb","sensors","settings","camera","four_grid","eight_grid"};
  for(int i=0;i<7;i++){
    const auto& def=getTemplateDefinition(pages[i].template_id);
    for(int j=0;j<def.max_slots;j++)assert(def.slots[j].rect.y>=44);
    assert(handlePageTouch(pages[i],20,30));assert(mapOpens==i+1);
    assert(handlePageTouch(pages[i],215,30));assert(rideOpens==i+1);
    assert(!handlePageTouch(pages[i],100,20));
    assert(!handlePageTouch(pages[i],100,310));
    renderPage(pages[i],i,7,state,true);
    if(argc>1){printf("SCREEN|%s\n",names[i]);for(auto& c:canvas.commands)puts(c.c_str());}
  }
  if(argc==1)puts("All page headers, inert title/status and template bounds passed");
}
