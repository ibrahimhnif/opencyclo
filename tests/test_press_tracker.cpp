#include "ui/gesture.h"
#include "ui/press_tracker.h"
#include <cassert>
#include <cstdio>

int main() {
  auto two = [](int x, int) { return x < 100 ? 0 : 1; };
  auto always1 = [](int, int) { return 1; };
  ui::PressTracker p;

  p.update(true, 10, 10, two);
  assert(p.pressed() == 0);
  assert(p.update(false, 0, 0, two) == 0);
  assert(p.pressed() == -1);

  // Moving past the 6px slop cancels.
  p.update(true, 10, 10, two);
  p.update(true, 17, 10, two);
  assert(p.update(false, 0, 0, two) == -1);
  p.update(true, 10, 10, two);
  p.update(true, 16, 4, two);
  assert(p.update(false, 0, 0, two) == 0);

  // Sliding onto another target cancels.
  p.update(true, 95, 10, two);
  p.update(true, 101, 10, two);
  assert(p.update(false, 0, 0, two) == -1);

  // The screen changed under the finger: the original point now hits another target.
  p.update(true, 10, 10, two);
  assert(p.update(false, 0, 0, always1) == -1);

  p.update(true, 10, 10, two);
  p.cancel();
  assert(p.update(false, 0, 0, two) == -1);

  // Pressing empty space fires nothing.
  auto none = [](int, int) { return -1; };
  p.update(true, 10, 10, none);
  assert(p.pressed() == -1 && p.update(false, 0, 0, none) == -1);

  assert(ui::classifyRelease(-36, 69) == ui::Gesture::SwipeLeft);
  assert(ui::classifyRelease(-35, 0) == ui::Gesture::None);
  assert(ui::classifyRelease(36, -69) == ui::Gesture::SwipeRight);
  assert(ui::classifyRelease(36, 70) == ui::Gesture::None);
  assert(ui::classifyRelease(19, -19) == ui::Gesture::Tap);
  assert(ui::classifyRelease(20, 0) == ui::Gesture::None);
  puts("press tracker and gesture tests passed");
}
