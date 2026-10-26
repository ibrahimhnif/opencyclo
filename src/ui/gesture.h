#pragma once
#include <cstdint>
#include <cstdlib>

namespace ui {

// Dashboard release gestures. Horizontal swipes change page; a short,
// near-stationary release is a tap delegated to the page.
enum class Gesture { None, SwipeLeft, SwipeRight, Tap };

const int kSwipeMinDx = 35;
const int kSwipeMaxDy = 70;
const int kTapMaxMove = 20;
// Releases closer together than this are ignored (touch-controller bounce).
const uint32_t kGestureDebounceMs = 150;

inline Gesture classifyRelease(int dx, int dy) {
  if (dx < -kSwipeMinDx && std::abs(dy) < kSwipeMaxDy) return Gesture::SwipeLeft;
  if (dx > kSwipeMinDx && std::abs(dy) < kSwipeMaxDy) return Gesture::SwipeRight;
  if (std::abs(dx) < kTapMaxMove && std::abs(dy) < kTapMaxMove) return Gesture::Tap;
  return Gesture::None;
}

}  // namespace ui
