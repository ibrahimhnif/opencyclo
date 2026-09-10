#include "power_menu.h"
#include "config/pins.h"
#include "hardware/battery.h"
#include "hardware/display.h"
#include "hardware/power.h"
#include "hardware/power_button.h"
#include "storage/settings.h"

static PowerButton button;
static bool menuOpen = false;
static bool screenOff = false;
static bool consumeUntilRelease = false;
static int pressedAction = -1;
static const char* message = nullptr;
static bool menuDirty = true;
static bool menuWasTouched = false;

void openPowerMenu() {
  menuOpen = true;
  message = nullptr;
  pressedAction = -1;
  consumeUntilRelease = true;
  menuDirty = true;
  menuWasTouched = false;
}

static void wakeScreen() {
  screenOff = false;
  tft.wakeup();
  setDisplayBrightness(g_settings.brightness);
  consumeUntilRelease = true;
}

static void drawMenu() {
  static uint32_t lastDrawMs = 0;
  if (!menuDirty && millis() - lastDrawMs < 2000) return;
  menuDirty = false;
  lastDrawMs = millis();
  tft.fillScreen(TFT_BLACK);
  tft.setTextPadding(0);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(12, 14);
  tft.print("POWER & BATTERY");
  tft.setTextSize(1);
  tft.setCursor(12, 50);
  tft.printf("Battery estimate: %u%%  %.2f V", readBatteryPercentage(), readBatteryVoltage());
  tft.setCursor(12, 72);
  if (hasUsbPowerSense()) {
    tft.print(isUsbPowerConnected() ? "USB power connected" : "USB power disconnected");
  } else {
    tft.print("USB charging is automatic.");
  }
  tft.setCursor(12, 90);
  tft.print("Charge status unavailable.");
  tft.setCursor(12, 116);
  tft.print("Power off / restart ends this ride.");
  const char* labels[] = {"POWER OFF", "RESTART", "BACK"};
  for (int i = 0; i < 3; ++i) {
    int y = 144 + i * 44;
    tft.fillRoundRect(12, y, 216, 36, 6, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextSize(2);
    tft.setCursor(24, y + 10);
    tft.print(labels[i]);
  }
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(12, 282);
  tft.print(message ? message : "Wake after power off: press BOOT");
}

static int actionAt(int16_t x, int16_t y) {
  if (x < 12 || x >= 228 || y < 144) return -1;
  int action = (y - 144) / 44;
  return action < 3 && (y - 144) % 44 < 36 ? action : -1;
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
    if (menuOpen) drawMenu();
    return true;
  }
  if (!menuOpen) return consumed;

  static int lastAction = -1;
  if (touched) {
    int action = actionAt(x, y);
    if (!menuWasTouched) pressedAction = action;
    if (action != pressedAction) pressedAction = -1;
    lastAction = action;
  } else if (menuWasTouched) {
    int action = pressedAction;
    pressedAction = -1;
    if (action >= 0 && action == lastAction) {
      if (action == 2) {
        menuOpen = false;
      } else {
        message = "Saving ride...";
        menuDirty = true;
        drawMenu();
        message = powerOff(action == 1);
        menuDirty = true;
      }
    }
  }
  menuWasTouched = touched;
  if (menuOpen) drawMenu();
  return true;
}
