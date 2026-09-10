#include "hardware/power_button.h"
#include <assert.h>
#include <stdio.h>

using Event = PowerButtonEvent;

int main() {
  // Wake/boot with BOOT held cannot immediately open a menu or shut down again.
  PowerButton held;
  assert(held.update(true, 0) == Event::None);
  assert(held.update(true, 3000) == Event::None);
  assert(held.update(false, 3100) == Event::None);
  assert(held.update(false, 3140) == Event::None);

  // Ignore contact bounce; one short press is emitted only on stable release.
  assert(held.update(true, 3200) == Event::None);
  assert(held.update(false, 3210) == Event::None);
  assert(held.update(false, 3250) == Event::None);
  assert(held.update(true, 3300) == Event::None);
  assert(held.update(true, 3340) == Event::None);
  assert(held.update(false, 3500) == Event::None);
  assert(held.update(false, 3540) == Event::ShortPress);
  assert(held.update(false, 3600) == Event::None);

  // A long press fires once and its release must not toggle the display.
  assert(held.update(true, 4000) == Event::None);
  assert(held.update(true, 4040) == Event::None);
  assert(held.update(true, 6039) == Event::None);
  assert(held.update(true, 6040) == Event::LongPress);
  assert(held.update(true, 7000) == Event::None);
  assert(held.update(false, 7100) == Event::None);
  assert(held.update(false, 7140) == Event::None);

  // millis() wraps after ~49 days; holding across that boundary still works.
  PowerButton rollover;
  assert(rollover.update(false, UINT32_MAX - 1000) == Event::None);
  assert(rollover.update(true, UINT32_MAX - 900) == Event::None);
  assert(rollover.update(true, UINT32_MAX - 860) == Event::None);
  assert(rollover.update(true, 1138) == Event::None);
  assert(rollover.update(true, 1139) == Event::LongPress);
  puts("Power button tests passed");
}
