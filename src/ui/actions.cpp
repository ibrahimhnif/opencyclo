#include "ui/actions.h"

#include "core/telemetry_state.h"
#include "hardware/power.h"
#include "navigation/navigation.h"
#include "ui/ride_menu.h"

ActionResult dispatchAction(ActionId id) {
  switch (id) {
    case ACT_OPEN_MAP:
      openNavigation();
      return {true, nullptr};
    case ACT_OPEN_RIDE_MENU:
      openRideMenu();
      return {true, nullptr};
    case ACT_RIDE_START:
    case ACT_RIDE_RESUME:
      setManualRideState(RIDE_STATE_ACTIVE);
      return {true, nullptr};
    case ACT_RIDE_PAUSE:
      setManualRideState(RIDE_STATE_PAUSED);
      return {true, nullptr};
    case ACT_RIDE_SAVE:
      return {requestFinishRide(), nullptr};
    case ACT_POWER_OFF:
    case ACT_POWER_RESTART:
      // Successful shutdown never returns; a returned message is the failure.
      return {true, powerOff(id == ACT_POWER_RESTART)};
    default:
      return {false, nullptr};
  }
}

const char* actionName(ActionId id) {
  static const char* const kNames[ACT_COUNT] = {"", "map.open", "ride.menu", "ride.start", "ride.pause",
                                                "ride.resume", "ride.save", "power.off", "power.restart"};
  return id < ACT_COUNT ? kNames[id] : "";
}
