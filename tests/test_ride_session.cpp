#include "ride_fakes.h"
#include "core/telemetry_state.h"
#include "ui/ride_menu.h"
#include "ui/ride_view.h"
#include <cassert>
uint32_t fakeMillis=1;
Canvas canvas;
bool label(const char* s){for(auto& l:canvas.labels)if(l.find(s)!=std::string::npos)return true;return false;}
void frame(){fakeMillis+=101;updateRideMenu(false,0,0);}
void tap(int y){updateRideMenu(true,30,y);frame();}
int main() {
  initTelemetryState();
  auto boot=getTelemetrySnapshot();
  assert(boot.ride_state==RIDE_STATE_IDLE && !boot.ride_auto_allowed);
  // Movement, GPS fix and a faulty/stale fusion proposal cannot start a ride
  // or accrue statistics before the user explicitly starts the session.
  boot.gps_has_fix=true;boot.speed_kmh=25;
  boot.ride_state=RIDE_STATE_ACTIVE;boot.ride_auto_allowed=true;
  boot.trip_distance_km=10;boot.ride_time_s=100;boot.max_speed_kmh=25;
  setFusionTelemetryState(boot);
  auto idle=getTelemetrySnapshot();
  assert(idle.ride_state==RIDE_STATE_IDLE && !idle.ride_auto_allowed);
  assert(idle.trip_distance_km==0 && idle.ride_time_s==0 && idle.max_speed_kmh==0);
  assert(idle.gps_has_fix && idle.speed_kmh==25); // Live map/sensors still work.
  assert(rideui::view(idle,false).primary.action==rideui::Action::Start);
  assert(!requestFinishRide());
  openRideMenu();tap(200);
  assert(getTelemetrySnapshot().ride_state==RIDE_STATE_ACTIVE);
  assert(getTelemetrySnapshot().ride_auto_allowed);
  auto autoPause=getTelemetrySnapshot();autoPause.ride_state=RIDE_STATE_PAUSED;
  setFusionTelemetryState(autoPause);assert(getTelemetrySnapshot().ride_state==RIDE_STATE_PAUSED);
  autoPause.ride_state=RIDE_STATE_ACTIVE;setFusionTelemetryState(autoPause);
  assert(getTelemetrySnapshot().ride_state==RIDE_STATE_ACTIVE);
  auto moving=getTelemetrySnapshot();
  moving.trip_distance_km=12.5;moving.ride_time_s=3600;
  moving.max_speed_kmh=28;setFusionTelemetryState(moving);
  // Dragging over Finish is not a tap.
  updateRideMenu(true,30,260);updateRideMenu(true,60,260);frame();
  assert(!label("Finish this ride?"));
  tap(260);assert(label("Finish this ride?"));
  tap(260);assert(getTelemetrySnapshot().ride_state==RIDE_STATE_ACTIVE);
  tap(260);tap(200);
  auto stopped=getTelemetrySnapshot();
  assert(stopped.ride_save==RIDE_SAVE_PENDING && stopped.ride_state==RIDE_STATE_IDLE);
  assert(!stopped.ride_auto_allowed && label("Saving GPX"));
  assert(!requestFinishRide() && !setManualRideState(RIDE_STATE_ACTIVE));
  moving.trip_distance_km=999;moving.ride_time_s=9999;
  setFusionTelemetryState(moving);setTelemetryState(moving);
  assert(getTelemetrySnapshot().trip_distance_km==12.5);
  assert(getTelemetrySnapshot().ride_save==RIDE_SAVE_PENDING);
  completeFinishRide(RIDE_SAVE_ERROR,"/rides/test.gpx");frame();
  assert(label("Save failed") && !setManualRideState(RIDE_STATE_ACTIVE));
  tap(200);assert(getTelemetrySnapshot().ride_save==RIDE_SAVE_PENDING);
  completeFinishRide(RIDE_SAVE_OK,"/rides/test.gpx");frame();
  assert(label("GPX saved") && label("/rides/test.gpx"));
  // Even a current-revision fusion update cannot auto-start a finished ride.
  auto autoStart=getTelemetrySnapshot();autoStart.ride_state=RIDE_STATE_ACTIVE;
  setFusionTelemetryState(autoStart);
  assert(getTelemetrySnapshot().ride_state==RIDE_STATE_IDLE);
  tap(260);
  assert(getTelemetrySnapshot().ride_state==RIDE_STATE_ACTIVE);
  assert(getTelemetrySnapshot().ride_time_s==0 && getTelemetrySnapshot().trip_distance_km==0);
  setFusionTelemetryState(moving);
  assert(getTelemetrySnapshot().trip_distance_km==0);
  tap(200);assert(getTelemetrySnapshot().ride_state==RIDE_STATE_PAUSED);
  assert(!getTelemetrySnapshot().ride_auto_allowed);
  tap(260);tap(200);completeFinishRide(RIDE_SAVE_NO_FILE,"");frame();
  assert(label("No GPX file saved") && !label("GPX saved"));
  using namespace rideui;
  Touch touch;
  auto s=getTelemetrySnapshot();
  auto summary=view(s,false);
  assert(hit(summary,primaryRect.x,primaryRect.y)==Action::Back);
  assert(hit(summary,228,184)==Action::None);
  assert(hit(summary,12,primaryRect.y+primaryRect.h)==Action::None);
  assert(hit(summary,12,secondaryRect.y)==Action::Start);
  touch.update(true,30,200,summary);
  s.ride_save=RIDE_SAVE_ERROR;
  assert(touch.update(false,0,0,view(s,false))==Action::None);
  s.ride_save=RIDE_SAVE_PENDING;
  auto saving=view(s,true);
  assert(saving.screen==Screen::Saving && !saving.back);
  assert(hit(saving,20,20)==Action::None && hit(saving,30,200)==Action::None);
  touch.update(true,30,260,summary);touch.cancel();
  assert(touch.update(false,0,0,summary)==Action::None);
  initTelemetryState(); // A reboot never restores automatic recording.
  assert(getTelemetrySnapshot().ride_state==RIDE_STATE_IDLE);
  assert(rideui::view(getTelemetrySnapshot(),false).primary.action==Action::Start);
  auto oldGps=getTelemetrySnapshot();
  oldGps.gps_has_fix=true;oldGps.speed_source=SPEED_SOURCE_GPS;oldGps.speed_kmh=139;
  auto invalid=getTelemetrySnapshot();
  setFusionTelemetryState(invalid);setTelemetryState(oldGps);
  assert(!getTelemetrySnapshot().gps_has_fix && getTelemetrySnapshot().speed_kmh==0);
  auto wheel=getTelemetrySnapshot();wheel.speed_source=SPEED_SOURCE_BLE_CSC;wheel.speed_kmh=24;
  setTelemetryState(wheel);setFusionTelemetryState(invalid);
  assert(getTelemetrySnapshot().speed_source==SPEED_SOURCE_BLE_CSC);
  assert(getTelemetrySnapshot().speed_kmh==24);
  puts("Ride lifecycle, stale snapshots, confirmation, retry and summary tests passed");
}
