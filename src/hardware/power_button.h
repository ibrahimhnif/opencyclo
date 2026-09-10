#pragma once

#include <stdint.h>

enum class PowerButtonEvent { None, ShortPress, LongPress };

// GPIO-independent so debounce, boot-held buttons and timer rollover can be tested.
class PowerButton {
 public:
  PowerButtonEvent update(bool pressed, uint32_t now) {
    if (pressed != rawPressed_) {
      rawPressed_ = pressed;
      changedAt_ = now;
    }
    if (now - changedAt_ < 40) return PowerButtonEvent::None;
    // A button held during boot/wakeup must be released before accepting input.
    if (!armed_) {
      if (!pressed) armed_ = true;
      return PowerButtonEvent::None;
    }
    if (pressed != stablePressed_) {
      stablePressed_ = pressed;
      if (pressed) {
        pressedAt_ = now;
        longSent_ = false;
      } else if (!longSent_) {
        return PowerButtonEvent::ShortPress;
      }
    }
    if (stablePressed_ && !longSent_ && now - pressedAt_ >= 2000) {
      longSent_ = true;
      return PowerButtonEvent::LongPress;
    }
    return PowerButtonEvent::None;
  }

 private:
  bool armed_ = false;
  bool rawPressed_ = false;
  bool stablePressed_ = false;
  bool longSent_ = false;
  uint32_t changedAt_ = 0;
  uint32_t pressedAt_ = 0;
};
