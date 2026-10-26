#pragma once
#include <cstdlib>

namespace ui {

// The one press/release rule every touch target shares. A press fires on
// release only if the finger stayed within kSlop of where it landed, never
// slid onto another target, and the landing point still hits the same target
// at release (so a screen change under the finger fires nothing).
// Target ids are >= 0; -1 means "no target".
class PressTracker {
 public:
  static const int kSlop = 6;

  void cancel() {
    down_ = false;
    cancelled_ = false;
    held_ = -1;
  }
  int pressed() const { return down_ && !cancelled_ ? held_ : -1; }

  // `hit(x, y)` returns the target id under a point. Returns the released
  // target's id, or -1.
  template <class Hit>
  int update(bool touched, int x, int y, Hit hit) {
    if (touched) {
      if (!down_) {
        down_ = true;
        cancelled_ = false;
        x0_ = x;
        y0_ = y;
        held_ = hit(x, y);
      }
      if (std::abs(x - x0_) > kSlop || std::abs(y - y0_) > kSlop || hit(x, y) != held_) cancelled_ = true;
      return -1;
    }
    const int result = down_ && !cancelled_ && held_ >= 0 && held_ == hit(x0_, y0_) ? held_ : -1;
    cancel();
    return result;
  }

 private:
  bool down_ = false;
  bool cancelled_ = false;
  int x0_ = 0;
  int y0_ = 0;
  int held_ = -1;
};

}  // namespace ui
