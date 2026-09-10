#include "charging_screen.h"
#include "config/pins.h"
#include "hardware/battery.h"
#include "hardware/display.h"
#include "hardware/power.h"
#include "hardware/power_button.h"
#include "storage/settings.h"

static void drawChargingScreen(bool sleepFailed) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextPadding(0);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(48, 30);
  tft.print("CHARGE MODE");
  tft.setTextSize(1);
  tft.setCursor(54, 62);
  tft.print(isUsbPowerConnected() ? "USB power connected" : "USB disconnected");

  uint8_t percent = readBatteryPercentage();
  tft.drawRoundRect(50, 94, 132, 64, 8, TFT_WHITE);
  tft.fillRect(182, 114, 8, 24, TFT_WHITE);
  int width = (120 * percent) / 100;
  if (width > 0) tft.fillRect(56, 100, width, 52, TFT_GREEN);
  tft.setTextSize(3);
  tft.setCursor(84, 174);
  tft.printf("%u%%", percent);
  tft.setTextSize(1);
  tft.setCursor(54, 208);
  tft.printf("Estimate  |  %.2f V", readBatteryVoltage());

  tft.fillRoundRect(28, 242, 184, 48, 8, TFT_GREEN);
  tft.setTextColor(TFT_BLACK, TFT_GREEN);
  tft.setTextSize(2);
  tft.setCursor(108, 258);
  tft.print("ON");
  if (sleepFailed) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(18, 304);
    tft.print("Sleep unavailable. Press ON.");
  }
}

void showChargingScreenIfNeeded() {
  if (!shouldStartChargingMode()) return;
  initDisplay();
  setDisplayBrightness(g_settings.brightness > 100 ? 100 : g_settings.brightness);

  PowerButton button;
  bool wasTouched = false;
  bool onPressed = false;
  bool touchArmed = false;
  bool sleepFailed = false;
  bool dirty = true;
  uint32_t lastDraw = 0;
  uint32_t lastUsbPresent = millis();

  for (;;) {
    uint32_t now = millis();
    auto event = button.update(digitalRead(PIN_POWER_BUTTON) == LOW, now);
    if (event != PowerButtonEvent::None) return;

    // No other I2C users or ride/BLE tasks exist in this mode.
    int16_t x = 0, y = 0;
    bool touched = tft.getTouch(&x, &y);
    bool insideOn = x >= 28 && x < 212 && y >= 242 && y < 290;
    if (!touchArmed) {
      if (!touched) touchArmed = true;
    } else if (touched) {
      if (!wasTouched) onPressed = insideOn;
      if (!insideOn) onPressed = false;
    } else if (wasTouched && onPressed) {
      return;
    }
    wasTouched = touched;

    if (isUsbPowerConnected()) {
      lastUsbPresent = now;
      sleepFailed = false;
    } else if (!sleepFailed && now - lastUsbPresent >= 250) {
      // Unplugging returns to off; USB reconnect or BOOT can wake again.
      sleepFailed = !sleepFromChargingMode();
      dirty = true;
    }

    if (dirty || now - lastDraw >= 2000) {
      drawChargingScreen(sleepFailed);
      dirty = false;
      lastDraw = now;
    }
    delay(50);
  }
}
