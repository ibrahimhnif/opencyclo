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
  assert(!requestFinishRide());
  openRideMenu();tap(200);
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
  puts("Ride lifecycle, stale snapshots, confirmation, retry and summary tests passed");
}
