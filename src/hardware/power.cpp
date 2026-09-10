#include "power.h"
#include "config/pins.h"
#include "hardware/ble_task.h"
#include "hardware/gps_task.h"
#include "hardware/display.h"
#include "storage/logger_task.h"
#include <atomic>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_attr.h>

static_assert(PIN_USB_POWER_SENSE == -1 || PIN_USB_POWER_SENSE == PIN_EXP_1 ||
              PIN_USB_POWER_SENSE == PIN_EXP_3 || PIN_USB_POWER_SENSE == PIN_EXP_4,
              "USB sense must use a free RTC expansion GPIO (2, 14 or 21)");

// Survives deep sleep; a cold boot/reset starts normally.
static RTC_DATA_ATTR bool poweredOff = false;

enum class Operation { Idle, Update, RouteSync, PowerOff };
static std::atomic<Operation> operation{Operation::Idle};

bool beginFirmwareUpdate() {
  Operation expected = Operation::Idle;
  return operation.compare_exchange_strong(expected, Operation::Update);
}

bool beginRouteSync() {
  Operation expected=Operation::Idle;
  return operation.compare_exchange_strong(expected,Operation::RouteSync);
}
void endRouteSync() {
  Operation expected=Operation::RouteSync;
  operation.compare_exchange_strong(expected,Operation::Idle);
}

void endFirmwareUpdate() {
  Operation expected = Operation::Update;
  operation.compare_exchange_strong(expected, Operation::Idle);
}

bool beginPowerOff() {
  Operation expected = Operation::Idle;
  return operation.compare_exchange_strong(expected, Operation::PowerOff);
}

void cancelPowerOff() {
  Operation expected = Operation::PowerOff;
  operation.compare_exchange_strong(expected, Operation::Idle);
}

bool isPowerOffRequested() { return operation.load() == Operation::PowerOff; }

bool hasUsbPowerSense() { return PIN_USB_POWER_SENSE >= 0; }

bool isUsbPowerConnected() {
#if PIN_USB_POWER_SENSE >= 0
  return digitalRead(PIN_USB_POWER_SENSE) == HIGH;
#else
  return false;
#endif
}

bool shouldStartChargingMode() {
  return poweredOff && hasUsbPowerSense() &&
         esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0;
}

void markPowerOn() { poweredOff = false; }

void initPower() {
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis(static_cast<gpio_num_t>(PIN_TFT_BL));
  rtc_gpio_deinit(static_cast<gpio_num_t>(PIN_POWER_BUTTON));
  pinMode(PIN_POWER_BUTTON, INPUT_PULLUP);
#if PIN_USB_POWER_SENSE >= 0
  rtc_gpio_deinit(static_cast<gpio_num_t>(PIN_USB_POWER_SENSE));
  pinMode(PIN_USB_POWER_SENSE, INPUT);
#endif
}

static bool configurePowerOffWake() {
  if (esp_sleep_enable_ext0_wakeup(
        static_cast<gpio_num_t>(PIN_POWER_BUTTON), 0) != ESP_OK) return false;
#if PIN_USB_POWER_SENSE >= 0
  rtc_gpio_pullup_dis(static_cast<gpio_num_t>(PIN_USB_POWER_SENSE));
  rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(PIN_USB_POWER_SENSE));
  if (esp_sleep_enable_ext1_wakeup(1ULL << PIN_USB_POWER_SENSE,
                                  ESP_EXT1_WAKEUP_ANY_HIGH) != ESP_OK) return false;
#endif
  return true;
}

static void enterPowerOffSleep() {
  poweredOff = true;
  // GPIO45 is not an RTC pin: explicitly hold the backlight off in deep sleep.
  ledcDetachPin(PIN_TFT_BL);
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, LOW);
  gpio_hold_en(static_cast<gpio_num_t>(PIN_TFT_BL));
  gpio_deep_sleep_hold_en();
  rtc_gpio_pullup_en(static_cast<gpio_num_t>(PIN_POWER_BUTTON));
  rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(PIN_POWER_BUTTON));
  esp_deep_sleep_start();
}

bool sleepFromChargingMode() {
  if (!configurePowerOffWake()) return false;
  tft.sleep();
  enterPowerOffSleep();
  return true;
}

const char* powerOff(bool restart) {
  if (!beginPowerOff()) return "Finish firmware update first";

  // The logger closes its own file. Never manipulate SD from the UI task.
  if (!prepareLoggerForPowerOff(5000)) {
    cancelPowerOff();
    return "SD busy. Try again.";
  }
  if (!prepareBleForPowerOff(5000)) {
    resumeLoggerAfterPowerOff();
    cancelPowerOff();
    return "Bluetooth busy. Try again.";
  }

  // Wait for release so the wake pin cannot immediately wake the device.
  uint32_t releasedAt = millis();
  uint32_t start = millis();
  while (millis() - releasedAt < 100) {
    if (digitalRead(PIN_POWER_BUTTON) == LOW) releasedAt = millis();
    if (millis() - start >= 5000) {
      resumeBleAfterPowerOff();
      resumeLoggerAfterPowerOff();
      cancelPowerOff();
      return "Release BOOT and try again";
    }
    delay(10);
  }

  if (!restart && !configurePowerOffWake()) {
    resumeBleAfterPowerOff();
    resumeLoggerAfterPowerOff();
    cancelPowerOff();
    return "Cannot enable button wake";
  }

  // Last fallible shutdown step. Restart keeps GNSS running; screen-off and
  // ride pause never call this path. Charging mode never starts the GPS task.
  if(!restart && !prepareGpsForPowerOff()) {
    resumeBleAfterPowerOff();
    resumeLoggerAfterPowerOff();
    cancelPowerOff();
    return "GPS standby failed. Retry.";
  }
  stopBleForPowerOff();
  tft.sleep();
  Serial.println(restart ? "[POWER] Restarting" : "[POWER] Deep sleep; BOOT wakes");
  if (restart) {
    markPowerOn();
    ESP.restart();
  }
  enterPowerOffSleep();
  return nullptr;
}
