#pragma once

#include <stdint.h>

namespace IdleDisplayLogic {

enum class Action : uint8_t {
  None,
  StartScreenSaver,
  DimScreen,
  ScreenOff,
};

class Controller {
 public:
  void begin(uint32_t nowMs) {
    lastActivityMs_ = nowMs;
    initialized_ = true;
  }

  void noteActivity(uint32_t nowMs) {
    begin(nowMs);
  }

  uint32_t idleMilliseconds(uint32_t nowMs) const {
    return initialized_ ? nowMs - lastActivityMs_ : 0;
  }

  Action update(uint32_t nowMs, bool eligible, bool screenSaverActive,
                bool screenDimmed, bool allowDim, bool allowScreenOff,
                uint16_t screenSaverMinutes, uint16_t dimMinutes,
                uint16_t screenOffMinutes) {
    if (!initialized_) {
      begin(nowMs);
    }

    if (!eligible) {
      noteActivity(nowMs);
      return Action::None;
    }

    const uint32_t elapsedMs = idleMilliseconds(nowMs);
    if (allowScreenOff && screenOffMinutes > 0 &&
        elapsedMs >= static_cast<uint32_t>(screenOffMinutes) * 60000UL) {
      return Action::ScreenOff;
    }
    if (allowDim && !screenDimmed && dimMinutes > 0 &&
        elapsedMs >= static_cast<uint32_t>(dimMinutes) * 60000UL) {
      return Action::DimScreen;
    }
    if (!screenSaverActive && screenSaverMinutes > 0 &&
        elapsedMs >= static_cast<uint32_t>(screenSaverMinutes) * 60000UL) {
      return Action::StartScreenSaver;
    }
    return Action::None;
  }

 private:
  uint32_t lastActivityMs_ = 0;
  bool initialized_ = false;
};

}  // namespace IdleDisplayLogic
