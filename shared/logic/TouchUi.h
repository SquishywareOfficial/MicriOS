#pragma once

#include <stdint.h>

namespace TouchUi {

struct Point {
  int16_t x = 0;
  int16_t y = 0;
};

struct Rect {
  int16_t x = 0;
  int16_t y = 0;
  int16_t w = 0;
  int16_t h = 0;

  bool contains(Point point) const {
    return contains(point.x, point.y);
  }

  bool contains(int16_t px, int16_t py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
  }
};

struct TouchSample {
  bool down = false;
  Point point;
};

enum class SwipeDirection : uint8_t {
  None,
  Left,
  Right,
  Up,
  Down
};

struct TouchEvent {
  bool down = false;
  bool pressed = false;
  bool released = false;
  bool tap = false;
  bool longPress = false;
  bool dragged = false;
  uint32_t holdMs = 0;
  Point point;
  Point start;
  SwipeDirection swipe = SwipeDirection::None;
};

constexpr int16_t NO_CONTROL = -1;

struct CaptureResult {
  int16_t id = NO_CONTROL;
  bool consumed = false;
  bool pressed = false;
  bool released = false;
  bool activated = false;
  bool longActivated = false;
};

// Captures a control from press until release. Activation only occurs when the
// finger is released over the same control, preventing drags and neighboring
// controls from leaking actions into one another.
class ControlCapture {
 public:
  CaptureResult update(const TouchSample& sample, const TouchEvent& event,
                       int16_t hitId) {
    CaptureResult result;
    if (event.pressed) {
      active_ = hitId;
      longActivated_ = false;
      result.id = active_;
      result.pressed = active_ != NO_CONTROL;
      result.consumed = result.pressed;
      return result;
    }

    if (active_ == NO_CONTROL) return result;
    result.id = active_;
    result.consumed = true;
    if (event.longPress && hitId == active_ && !longActivated_) {
      longActivated_ = true;
      result.longActivated = true;
    }
    if (event.released) {
      result.released = true;
      result.activated = hitId == active_ && !event.dragged &&
                         !longActivated_;
      active_ = NO_CONTROL;
      longActivated_ = false;
    }
    return result;
  }

  bool isPressed(int16_t id, const TouchSample& sample) const {
    return sample.down && active_ == id;
  }

  int16_t activeId() const { return active_; }
  void reset() {
    active_ = NO_CONTROL;
    longActivated_ = false;
  }

 private:
  int16_t active_ = NO_CONTROL;
  bool longActivated_ = false;
};

inline Point currentPoint(const TouchSample& sample, const TouchEvent& event) {
  return sample.down ? sample.point : event.point;
}

inline int16_t hitRectList(const Rect* rects, uint8_t count, Point point) {
  for (uint8_t i = 0; i < count; ++i) {
    if (rects[i].contains(point)) return i;
  }
  return NO_CONTROL;
}

class Tracker {
 public:
  explicit Tracker(uint16_t tapSlop = 14,
                   uint16_t swipeDistance = 36,
                   uint16_t longPressMs = 650)
      : tapSlop_(tapSlop),
        swipeDistance_(swipeDistance),
        longPressMs_(longPressMs) {}

  void reset(const TouchSample& sample, uint32_t nowMs) {
    wasDown_ = sample.down;
    longPressEmitted_ = false;
    dragged_ = false;
    start_ = sample.point;
    current_ = sample.point;
    pressedAtMs_ = nowMs;
  }

  TouchEvent update(const TouchSample& sample, uint32_t nowMs) {
    TouchEvent event;
    event.down = sample.down;
    event.point = sample.down ? sample.point : current_;
    event.start = start_;

    if (sample.down && !wasDown_) {
      event.pressed = true;
      start_ = sample.point;
      current_ = sample.point;
      pressedAtMs_ = nowMs;
      longPressEmitted_ = false;
      dragged_ = false;
      event.start = start_;
      event.point = current_;
    } else if (sample.down) {
      current_ = sample.point;
      if (!withinDistance(current_, tapSlop_)) {
        dragged_ = true;
      }
      event.point = current_;
      event.start = start_;
      event.holdMs = nowMs - pressedAtMs_;
      if (!longPressEmitted_ && !dragged_ && event.holdMs >= longPressMs_) {
        longPressEmitted_ = true;
        event.longPress = true;
      }
    } else if (!sample.down && wasDown_) {
      event.released = true;
      event.point = current_;
      event.start = start_;
      event.holdMs = nowMs - pressedAtMs_;
      event.swipe = swipeFor(current_);
      event.tap = !longPressEmitted_ && event.swipe == SwipeDirection::None &&
                  withinDistance(current_, tapSlop_);
    }

    event.dragged = dragged_;
    wasDown_ = sample.down;
    return event;
  }

 private:
  bool withinDistance(Point point, uint16_t distance) const {
    const int32_t dx = static_cast<int32_t>(point.x) - start_.x;
    const int32_t dy = static_cast<int32_t>(point.y) - start_.y;
    return dx * dx + dy * dy <= static_cast<int32_t>(distance) * distance;
  }

  SwipeDirection swipeFor(Point point) const {
    const int32_t dx = static_cast<int32_t>(point.x) - start_.x;
    const int32_t dy = static_cast<int32_t>(point.y) - start_.y;
    const int32_t absX = dx < 0 ? -dx : dx;
    const int32_t absY = dy < 0 ? -dy : dy;
    if (absX < swipeDistance_ && absY < swipeDistance_) {
      return SwipeDirection::None;
    }
    if (absX >= absY) {
      return dx < 0 ? SwipeDirection::Left : SwipeDirection::Right;
    }
    return dy < 0 ? SwipeDirection::Up : SwipeDirection::Down;
  }

  uint16_t tapSlop_;
  uint16_t swipeDistance_;
  uint16_t longPressMs_;
  bool wasDown_ = false;
  bool longPressEmitted_ = false;
  bool dragged_ = false;
  uint32_t pressedAtMs_ = 0;
  Point start_;
  Point current_;
};

enum class LogicalButton : uint8_t {
  None,
  B1,
  B2
};

enum class ControlMode : uint8_t {
  Direct,
  OneButton,
  SplitButtons
};

enum class SplitAxis : uint8_t {
  X,
  Y
};

struct ControlProfile {
  ControlMode mode = ControlMode::OneButton;
  LogicalButton left = LogicalButton::B1;
  LogicalButton right = LogicalButton::B2;
  SplitAxis splitAxis = SplitAxis::X;
};

struct LogicalInputState {
  bool b1Down = false;
  bool b2Down = false;
  bool backRequested = false;
};

inline void setButton(LogicalInputState& state, LogicalButton button, bool down) {
  if (button == LogicalButton::B1) state.b1Down = state.b1Down || down;
  if (button == LogicalButton::B2) state.b2Down = state.b2Down || down;
}

inline LogicalInputState mapGameTouch(const TouchSample& sample,
                                      const Rect& content,
                                      const ControlProfile& profile) {
  LogicalInputState state;
  if (!sample.down || !content.contains(sample.point) ||
      profile.mode == ControlMode::Direct) {
    return state;
  }
  if (profile.mode == ControlMode::OneButton) {
    state.b1Down = true;
    return state;
  }
  const bool firstHalf = profile.splitAxis == SplitAxis::X
                             ? sample.point.x < content.x + content.w / 2
                             : sample.point.y < content.y + content.h / 2;
  setButton(state, firstHalf ? profile.left : profile.right, true);
  return state;
}

struct LayoutMetrics {
  int16_t width = 320;
  int16_t height = 240;
  int16_t systemBarHeight = 32;

  Rect contentRect() const {
    return {0, 0, width, static_cast<int16_t>(height - systemBarHeight)};
  }

  Rect systemBarRect() const {
    return {0, static_cast<int16_t>(height - systemBarHeight), width, systemBarHeight};
  }
};

struct GridMetrics {
  Rect bounds;
  int16_t gapX = 8;
  int16_t gapY = 8;
  uint8_t columns = 2;
  uint8_t rows = 3;

  uint8_t capacity() const {
    return columns * rows;
  }

  Rect cardRect(uint8_t visibleIndex) const {
    const int16_t cardW = (bounds.w - gapX * (columns - 1)) / columns;
    const int16_t cardH = (bounds.h - gapY * (rows - 1)) / rows;
    const uint8_t column = visibleIndex % columns;
    const uint8_t row = visibleIndex / columns;
    return {static_cast<int16_t>(bounds.x + column * (cardW + gapX)),
            static_cast<int16_t>(bounds.y + row * (cardH + gapY)),
            cardW,
            cardH};
  }
};

inline int16_t cardAt(const GridMetrics& metrics,
                      uint16_t itemCount,
                      uint16_t pageOffset,
                      Point point) {
  for (uint8_t slot = 0; slot < metrics.capacity(); ++slot) {
    const uint16_t index = pageOffset + slot;
    if (index >= itemCount) break;
    if (metrics.cardRect(slot).contains(point)) {
      return static_cast<int16_t>(index);
    }
  }
  return -1;
}

inline uint16_t pageOffset(uint16_t page, uint8_t capacity) {
  return capacity == 0 ? 0 : page * capacity;
}

inline uint16_t pageCount(uint16_t itemCount, uint8_t capacity) {
  return capacity == 0 ? 0 : (itemCount + capacity - 1) / capacity;
}

}  // namespace TouchUi
