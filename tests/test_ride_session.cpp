#include "ride_fakes.h"
#include "core/telemetry_state.h"
#include "ui/ride_menu.h"
#include <cassert>
uint32_t fakeMillis=1;
Canvas canvas;
bool label(const char* s){for(auto& l:canvas.labels)if(l.find(s)!=std::string::npos)return true;return false;}
void frame(){fakeMillis+=101;updateRideMenu(false,0,0);}
void tap(int y){updateRideMenu(true,30,y);frame();}
int main() {
  initTelemetryState();
  assert(!requestFinishRide());
  openRideMenu();tap(120);
  assert(getTelemetrySnapshot().ride_state==RIDE_STATE_ACTIVE);
  auto moving=getTelemetrySnapshot();
  moving.trip_distance_km=12.5;moving.ride_time_s=3600;
  moving.max_speed_kmh=28;setFusionTelemetryState(moving);
  // Dragging over Finish is not a tap.
  updateRideMenu(true,30,180);updateRideMenu(true,60,180);frame();
  assert(!label("FINISH RIDE?"));
  tap(180);assert(label("FINISH RIDE?"));
  tap(270);assert(getTelemetrySnapshot().ride_state==RIDE_STATE_ACTIVE);
  tap(180);tap(180);
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
  tap(180);assert(getTelemetrySnapshot().ride_save==RIDE_SAVE_PENDING);
  completeFinishRide(RIDE_SAVE_OK,"/rides/test.gpx");frame();
  assert(label("GPX saved") && label("/rides/test.gpx"));
  // Even a current-revision fusion update cannot auto-start a finished ride.
  auto autoStart=getTelemetrySnapshot();autoStart.ride_state=RIDE_STATE_ACTIVE;
  setFusionTelemetryState(autoStart);
  assert(getTelemetrySnapshot().ride_state==RIDE_STATE_IDLE);
  tap(120);
  assert(getTelemetrySnapshot().ride_state==RIDE_STATE_ACTIVE);
  assert(getTelemetrySnapshot().ride_time_s==0 && getTelemetrySnapshot().trip_distance_km==0);
  setFusionTelemetryState(moving);
  assert(getTelemetrySnapshot().trip_distance_km==0);
  tap(120);assert(getTelemetrySnapshot().ride_state==RIDE_STATE_PAUSED);
  assert(!getTelemetrySnapshot().ride_auto_allowed);
  tap(180);tap(180);completeFinishRide(RIDE_SAVE_NO_FILE,"");frame();
  assert(label("no open GPX") && !label("GPX saved"));
  puts("Ride lifecycle, stale snapshots, confirmation, retry and summary tests passed");
}
