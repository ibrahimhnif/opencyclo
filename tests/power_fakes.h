#pragma once
#include <stdint.h>
#include <string>
#include <vector>

extern std::vector<std::string> calls;
extern uint32_t fakeTime;
extern bool loggerReady, bleReady, buttonDown, wakeReady;
extern bool gpsReady;
extern bool fakeUsbConnected, usbWakeReady;
extern int fakeWakeCause;
struct FakeTouch { bool touched; int16_t x, y; };
extern std::vector<FakeTouch> touches;
extern size_t touchIndex;
#define RTC_DATA_ATTR
enum { LOW = 0, HIGH = 1, INPUT_PULLUP = 1, OUTPUT = 2, INPUT = 3, ESP_OK = 0,
       ESP_SLEEP_WAKEUP_EXT0 = 4, ESP_SLEEP_WAKEUP_EXT1 = 5, ESP_EXT1_WAKEUP_ANY_HIGH = 6 };
using gpio_num_t = int;
inline uint32_t millis() { return fakeTime; }
inline void delay(uint32_t ms) { fakeTime += ms; }
inline int digitalRead(int pin) { return pin == 0 ? (buttonDown ? LOW : HIGH) : (fakeUsbConnected ? HIGH : LOW); }
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) { calls.push_back("backlight low"); }
inline void ledcDetachPin(int) {}
inline void gpio_deep_sleep_hold_dis() {}
inline void gpio_hold_dis(int) {}
inline void gpio_hold_en(int) { calls.push_back("hold backlight"); }
inline void gpio_deep_sleep_hold_en() {}
inline void rtc_gpio_deinit(int) {}
inline void rtc_gpio_pullup_en(int) {}
inline void rtc_gpio_pullup_dis(int) {}
inline void rtc_gpio_pulldown_dis(int) {}
inline int esp_sleep_enable_ext0_wakeup(int pin, int level) {
  calls.push_back("wake " + std::to_string(pin) + ":" + std::to_string(level));
  return wakeReady ? ESP_OK : -1;
}
inline int esp_sleep_enable_ext1_wakeup(uint64_t mask, int) {
  calls.push_back("usb wake " + std::to_string(mask));
  return usbWakeReady ? ESP_OK : -1;
}
inline int esp_sleep_get_wakeup_cause() { return fakeWakeCause; }
struct SleepReached {};
struct RestartReached {};
inline void esp_deep_sleep_start() { calls.push_back("sleep"); throw SleepReached{}; }
inline bool prepareLoggerForPowerOff(uint32_t) {
  calls.push_back("close log"); return loggerReady;
}
inline void resumeLoggerAfterPowerOff() { calls.push_back("resume log"); }
inline bool prepareBleForPowerOff(uint32_t) {
  calls.push_back("pause ble"); return bleReady;
}
inline void resumeBleAfterPowerOff() { calls.push_back("resume ble"); }
inline void stopBleForPowerOff() { calls.push_back("stop ble"); }
inline bool prepareGpsForPowerOff() {calls.push_back("gps standby");return gpsReady;}
enum { TFT_BLACK, TFT_WHITE, TFT_GREEN };
struct FakeDisplay {
  void sleep() { calls.push_back("display sleep"); }
  void fillScreen(int) {}
  void setTextPadding(int) {}
  void setTextSize(int) {}
  void setTextColor(int, int) {}
  void setCursor(int, int) {}
  void print(const char*) {}
  template<typename... Args> void printf(const char*, Args...) {}
  void drawRoundRect(int, int, int, int, int, int) {}
  void fillRoundRect(int, int, int, int, int, int) {}
  void fillRect(int, int, int, int, int) {}
  bool getTouch(int16_t* x, int16_t* y) {
    if (touchIndex >= touches.size()) return false;
    const auto& touch = touches[touchIndex++];
    *x = touch.x; *y = touch.y;
    return touch.touched;
  }
};
inline void initDisplay() { calls.push_back("init display"); }
inline void setDisplayBrightness(uint8_t) {}
inline uint8_t readBatteryPercentage() { return 63; }
inline float readBatteryVoltage() { return 3.85f; }
struct FakeSettings { uint8_t brightness = 200; };
extern FakeSettings g_settings;
struct FakeSerial { void println(const char*) {} };
struct FakeEsp { void restart() { calls.push_back("restart"); throw RestartReached{}; } };
extern FakeDisplay tft;
extern FakeSerial Serial;
extern FakeEsp ESP;
