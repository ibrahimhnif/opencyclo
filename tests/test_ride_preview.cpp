#include "ride_fakes.h"
#include "core/telemetry_state.h"
#include "ui/ride_menu.h"
uint32_t fakeMillis=1;
Canvas canvas;
int main() {
  const char* names[]={"ready","recording","paused","confirm","saving","saved","error","no_file","long_values"};
  for(int i=0;i<9;i++) {
    initTelemetryState();
    g_telemetry.trip_distance_km=i==8?9999:42.6;
    g_telemetry.ride_time_s=i==8?360000:5382;
    g_telemetry.avg_speed_kmh=i==8?999:28.5;
    if(i!=0)g_telemetry.ride_state=RIDE_STATE_ACTIVE;
    if(i==2)g_telemetry.ride_state=RIDE_STATE_PAUSED;
    if(i==4)g_telemetry.ride_save=RIDE_SAVE_PENDING;
    if(i==5)g_telemetry.ride_save=RIDE_SAVE_OK;
    if(i==6)g_telemetry.ride_save=RIDE_SAVE_ERROR;
    if(i==7)g_telemetry.ride_save=RIDE_SAVE_NO_FILE;
    strcpy(g_telemetry.ride_file,"/rides/20260910_124500.gpx");
    openRideMenu();
    if(i==3)updateRideMenu(true,30,260);
    fakeMillis+=300;updateRideMenu(false,0,0);
    printf("SCREEN|%s\n",names[i]);
    for(auto& c:canvas.commands)puts(c.c_str());
  }
}
