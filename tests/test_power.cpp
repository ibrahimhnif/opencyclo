#include "power_fakes.h"
#include "hardware/power.h"
#include "config/pins.h"
#include "ui/charging_screen.h"
#include <assert.h>
#include <algorithm>
#include <stdio.h>

std::vector<std::string> calls;
uint32_t fakeTime = 0;
bool loggerReady = true, bleReady = true, buttonDown = false, wakeReady = true;
bool fakeUsbConnected = false, usbWakeReady = true;
bool gpsReady=true;
int fakeWakeCause = 0;
std::vector<FakeTouch> touches;
size_t touchIndex = 0;
FakeSettings g_settings;
FakeDisplay tft;
FakeSerial Serial;
FakeEsp ESP;

static void reset() {
  cancelPowerOff();
  endFirmwareUpdate();
  endRouteSync();
  markPowerOn();
  calls.clear();
  fakeTime = 0;
  loggerReady = bleReady = wakeReady = true;
  gpsReady=true;
  buttonDown = false;
  fakeUsbConnected = false;
  usbWakeReady = true;
  fakeWakeCause = 0;
  touches.clear();
  touchIndex = 0;
}

static bool called(const char* call) {
  return std::find(calls.begin(), calls.end(), call) != calls.end();
}

int main() {
  reset();
  assert(beginRouteSync());
  assert(!beginFirmwareUpdate());
  assert(!beginPowerOff());
  endFirmwareUpdate();
  assert(!beginRouteSync());
  endRouteSync();
  assert(beginFirmwareUpdate());
  endRouteSync();
  assert(!beginPowerOff());
  reset();
  assert(beginFirmwareUpdate());
  assert(!beginFirmwareUpdate());
  assert(powerOff(false) != nullptr);
  assert(calls.empty()); // No logger or hardware changes during OTA.
  cancelPowerOff();
  assert(!beginPowerOff()); // Cancellation must not clear the OTA lock.
  endFirmwareUpdate();
  assert(beginPowerOff());
  endFirmwareUpdate();
  assert(!beginFirmwareUpdate()); // OTA completion cannot clear shutdown's lock.

  reset();
  loggerReady = false;
  assert(powerOff(false) != nullptr);
  assert(!called("pause ble") && !called("sleep"));
  assert(!isPowerOffRequested());

  reset();
  bleReady = false;
  assert(powerOff(false) != nullptr);
  assert(called("resume log") && !called("stop ble"));
  assert(!isPowerOffRequested());

  reset();
  buttonDown = true;
  assert(powerOff(false) != nullptr);
  assert(fakeTime >= 5000);
  assert(called("resume ble") && called("resume log"));
  assert(!called("stop ble") && !isPowerOffRequested());

  reset();
  wakeReady = false;
  assert(powerOff(false) != nullptr);
  assert(called("resume ble") && called("resume log"));
  assert(!called("stop ble") && !isPowerOffRequested());

  reset();gpsReady=false;
  assert(powerOff(false)!=nullptr);
  assert(called("gps standby") && called("resume log") && called("resume ble"));
  assert(!called("stop ble") && !called("sleep") && !isPowerOffRequested());

  reset();
  try { powerOff(false); assert(false); } catch (const SleepReached&) {}
  const std::vector<std::string> sleepOrder = {
    "close log", "pause ble", "wake 0:0",
#if PIN_USB_POWER_SENSE >= 0
    "usb wake " + std::to_string(1ULL << PIN_USB_POWER_SENSE),
#endif
    "gps standby", "stop ble", "display sleep",
    "backlight low", "hold backlight", "sleep"
  };
  assert(calls == sleepOrder);
  assert(fakeTime >= 100); // Wake requires a stable button release.

#if PIN_USB_POWER_SENSE >= 0
  fakeWakeCause = ESP_SLEEP_WAKEUP_EXT1;
  fakeUsbConnected = true;
  assert(isUsbPowerConnected());
  assert(shouldStartChargingMode()); // Plugging in after off shows charging UI.

  // Touch held at entry cannot turn on. Dragging out of ON cancels the tap.
  // Only the final deliberate ON tap exits the charging loop.
  touches = {{true, 100, 260}, {false, 0, 0},
             {true, 100, 260}, {true, 10, 100}, {false, 0, 0},
             {true, 100, 260}, {false, 0, 0}};
  calls.clear();
  showChargingScreenIfNeeded();
  assert(touchIndex == touches.size());
  assert(called("init display"));
  assert(!called("close log") && !called("stop ble"));

  fakeWakeCause = ESP_SLEEP_WAKEUP_EXT0;
  assert(!shouldStartChargingMode()); // BOOT explicitly turns the device on.
  fakeWakeCause = ESP_SLEEP_WAKEUP_EXT1;
  fakeUsbConnected = false;
  assert(shouldStartChargingMode()); // Fast unplug must return to sleep, not ride.
  calls.clear();
  try { showChargingScreenIfNeeded(); assert(false); } catch (const SleepReached&) {}
  assert(called("sleep"));
  assert(!called("close log") && !called("stop ble")); // Tasks were never started.
  markPowerOn();
  assert(!shouldStartChargingMode()); // ON exits charging mode.

  reset();
  usbWakeReady = false;
  assert(powerOff(false) != nullptr);
  assert(called("resume ble") && called("resume log"));
  assert(!called("stop ble") && !isPowerOffRequested());
#else
  assert(!hasUsbPowerSense());
  assert(!isUsbPowerConnected());
  assert(!shouldStartChargingMode());
#endif

  reset();
  try { powerOff(true); assert(false); } catch (const RestartReached&) {}
  const std::vector<std::string> restartOrder = {
    "close log", "pause ble", "stop ble", "display sleep", "restart"
  };
  assert(calls == restartOrder);
  puts("Power shutdown and OTA exclusion tests passed");
}
