#include "ride_fakes.h"
#include "core/telemetry_state.h"
#include "ui/actions.h"
#include <cassert>

uint32_t fakeMillis = 1;
Canvas canvas;
int navOpens = 0, rideMenuOpens = 0, powerOffCalls = 0;
bool lastRestartFlag = false;
void openNavigation() { navOpens++; }
void openRideMenu() { rideMenuOpens++; }
const char* powerOff(bool restart) {
  powerOffCalls++;
  lastRestartFlag = restart;
  return "Saving failed";
}

int main() {
  initTelemetryState();
  assert(dispatchAction(ACT_RIDE_START).ok && getTelemetrySnapshot().ride_state == RIDE_STATE_ACTIVE);
  assert(dispatchAction(ACT_RIDE_PAUSE).ok && getTelemetrySnapshot().ride_state == RIDE_STATE_PAUSED);
  assert(dispatchAction(ACT_RIDE_RESUME).ok && getTelemetrySnapshot().ride_state == RIDE_STATE_ACTIVE);
  assert(dispatchAction(ACT_RIDE_SAVE).ok && getTelemetrySnapshot().ride_save == RIDE_SAVE_PENDING);
  assert(dispatchAction(ACT_OPEN_MAP).ok && navOpens == 1);
  assert(dispatchAction(ACT_OPEN_RIDE_MENU).ok && rideMenuOpens == 1);
  ActionResult r = dispatchAction(ACT_POWER_RESTART);
  assert(r.ok && lastRestartFlag && !strcmp(r.message, "Saving failed"));
  r = dispatchAction(ACT_POWER_OFF);
  assert(!lastRestartFlag && powerOffCalls == 2);
  assert(!dispatchAction(ACT_NONE).ok && !dispatchAction(ActionId(ACT_COUNT)).ok);
  assert(!strcmp(actionName(ACT_RIDE_SAVE), "ride.save") && !strcmp(actionName(ACT_OPEN_MAP), "map.open"));
  assert(!strcmp(actionName(ACT_NONE), "") && !strcmp(actionName(ActionId(200)), ""));
  puts("action tests passed");
}
