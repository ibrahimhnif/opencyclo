#include "power_menu.h"
#include "control_layout.h"
#include "config/pins.h"
#include "hardware/battery.h"
#include "hardware/power.h"
#include "hardware/display.h"
#include "hardware/power_button.h"
#include "storage/settings.h"
#include "ui/actions.h"
#include "ui/frame_present.h"
#include "ui/power_screen.h"
#include "ui/press_tracker.h"

static PowerButton button;
static bool menuOpen = false;
static bool screenOff = false;
static bool consumeUntilRelease = false;
static const char* message = nullptr;
static bool menuDirty = true;
static ui::PressTracker press;
static int drawnPressed = -1;

void openPowerMenu() {
  menuOpen = true;
  message = nullptr;
  press.cancel();
  consumeUntilRelease = true;
  menuDirty = true;
}

static void wakeScreen() {
  screenOff = false;
  tft.wakeup();
  setDisplayBrightness(g_settings.brightness);
  consumeUntilRelease = true;
}

// Redraws on change (content or held button) and every 2 s for the battery.
static void drawMenu() {
  static uint32_t lastDrawMs = 0;
  if (!menuDirty && press.pressed() == drawnPressed && millis() - lastDrawMs < 2000) return;
  menuDirty = false;
  lastDrawMs = millis();
  drawnPressed = press.pressed();
  const powerui::View v = powerui::view(readBatteryPercentage(), readBatteryVoltage(), hasUsbPowerSense(),
                                        isUsbPowerConnected(), message);
  drawPowerScreen(canvas, v, drawnPressed, ui::activeTheme());
  presentFrame();
}

bool updatePowerUi(bool touched, int16_t x, int16_t y, uint32_t now) {
  bool consumed = menuOpen || screenOff || consumeUntilRelease;
  PowerButtonEvent event = button.update(digitalRead(PIN_POWER_BUTTON) == LOW, now);
  if (event == PowerButtonEvent::LongPress) {
    if (screenOff) wakeScreen();
    openPowerMenu();
    consumed = true;
  } else if (event == PowerButtonEvent::ShortPress) {
    if (screenOff) {
      wakeScreen();
    } else if (menuOpen) {
      menuOpen = false;
      consumeUntilRelease = true;
    } else {
      screenOff = true;
      tft.sleep();
    }
    consumed = true;
  }

  if (screenOff) {
    if (touched) wakeScreen();
    return true;
  }
  if (consumeUntilRelease) {
    if (!touched) consumeUntilRelease = false;
    press.cancel();
    if (menuOpen) drawMenu();
    return true;
  }
  if (!menuOpen) return consumed;

  const int released = press.update(touched, x, y, [](int px, int py) { return powerui::hit(px, py); });
  if (released == 2) {
    menuOpen = false;
  } else if (released >= 0) {
    message = "Saving ride...";
    menuDirty = true;
    drawMenu();
    message = dispatchAction(released == 0 ? ACT_POWER_OFF : ACT_POWER_RESTART).message;
    menuDirty = true;
  }
  if (menuOpen) drawMenu();
  return true;
}
