#pragma once
#include <stdint.h>

// Every side effect the UI can cause goes through dispatchAction(): screens
// and components only report which action was chosen. This is the single
// file where UI code reaches ride, navigation and power logic.
enum ActionId : uint8_t {
  ACT_NONE = 0,
  ACT_OPEN_MAP,
  ACT_OPEN_RIDE_MENU,
  ACT_RIDE_START,
  ACT_RIDE_PAUSE,
  ACT_RIDE_RESUME,
  ACT_RIDE_SAVE,
  ACT_POWER_OFF,
  ACT_POWER_RESTART,
  ACT_COUNT
};

struct ActionResult {
  bool ok;              // false: unknown action, or the request was refused
  const char* message;  // optional text for the screen to show, else nullptr
};

ActionResult dispatchAction(ActionId id);
// Stable wire name ("ride.start"); "" for ACT_NONE or an unknown id.
const char* actionName(ActionId id);
