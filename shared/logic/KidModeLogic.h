#pragma once

#include <stddef.h>
#include <stdint.h>

namespace KidMode {

constexpr uint8_t PIN_LENGTH = 6;
constexpr uint32_t MAX_TIMED_MINUTES = 120;
constexpr uint32_t AUTH_LOCKOUT_SECONDS = 30;
constexpr uint8_t SPLASH_TEXT_MAX_LENGTH = 24;
constexpr const char* DEFAULT_SPLASH_TEXT = "Child's Computer";

enum class SplashPalette : uint8_t {
  Candy = 0,
  Ocean,
  Space,
  Forest,
  Sunset,
  Rainbow,
  Count
};

constexpr const char* splashPaletteLabel(SplashPalette palette) {
  switch (palette) {
    case SplashPalette::Candy: return "Candy";
    case SplashPalette::Ocean: return "Ocean";
    case SplashPalette::Space: return "Space";
    case SplashPalette::Forest: return "Forest";
    case SplashPalette::Sunset: return "Sunset";
    case SplashPalette::Rainbow: return "Rainbow";
    default: return "Candy";
  }
}

constexpr bool validSplashPalette(SplashPalette palette) {
  return static_cast<uint8_t>(palette) <
         static_cast<uint8_t>(SplashPalette::Count);
}

constexpr bool validSplashCharacter(char value) {
  return (value >= 'A' && value <= 'Z') ||
         (value >= 'a' && value <= 'z') ||
         (value >= '0' && value <= '9') || value == ' ' || value == '\'' ||
         value == '-' || value == '&';
}

class SplashTextEditor {
 public:
  constexpr void begin(const char* initial = DEFAULT_SPLASH_TEXT) {
    clear();
    if (initial == nullptr) return;
    for (size_t i = 0; initial[i] != '\0'; ++i) append(initial[i]);
  }

  constexpr void clear() {
    length_ = 0;
    value_[0] = '\0';
  }

  constexpr bool append(char value) {
    if (!validSplashCharacter(value) || length_ >= SPLASH_TEXT_MAX_LENGTH) {
      return false;
    }
    value_[length_++] = value;
    value_[length_] = '\0';
    return true;
  }

  constexpr bool backspace() {
    if (length_ == 0) return false;
    value_[--length_] = '\0';
    return true;
  }

  constexpr bool empty() const { return length_ == 0; }
  constexpr uint8_t length() const { return length_; }
  constexpr const char* value() const { return value_; }

 private:
  char value_[SPLASH_TEXT_MAX_LENGTH + 1] = {};
  uint8_t length_ = 0;
};

struct SplashSettings {
  bool enabled = false;
  SplashPalette palette = SplashPalette::Candy;
  char text[SPLASH_TEXT_MAX_LENGTH + 1] = "Child's Computer";
};

enum class State : uint8_t {
  Disabled,
  Locked,
  Timed,
  Unlimited
};

enum class TickEvent : uint8_t {
  None,
  RemainingChanged,
  Expired
};

class Service {
 public:
  constexpr void begin(bool enabled) {
    state_ = enabled ? State::Locked : State::Disabled;
    deadlineUs_ = 0;
    remainingKey_ = 0;
    expiryReported_ = false;
  }

  constexpr void setEnabled(bool enabled) { begin(enabled); }

  constexpr bool unlockForMinutes(uint32_t minutes, uint64_t nowUs) {
    if (state_ == State::Disabled || minutes == 0 ||
        minutes > MAX_TIMED_MINUTES) {
      return false;
    }
    deadlineUs_ = nowUs + static_cast<uint64_t>(minutes) * 60ULL * 1000000ULL;
    state_ = State::Timed;
    expiryReported_ = false;
    remainingKey_ = remainingDisplayKey(nowUs);
    return true;
  }

  constexpr bool unlockUnlimited() {
    if (state_ == State::Disabled) return false;
    state_ = State::Unlimited;
    deadlineUs_ = 0;
    remainingKey_ = UINT32_MAX;
    expiryReported_ = false;
    return true;
  }

  constexpr void lockNow() {
    if (state_ == State::Disabled) return;
    state_ = State::Locked;
    deadlineUs_ = 0;
    remainingKey_ = 0;
    expiryReported_ = false;
  }

  constexpr TickEvent tick(uint64_t nowUs) {
    if (state_ != State::Timed) return TickEvent::None;
    if (nowUs >= deadlineUs_) {
      state_ = State::Locked;
      deadlineUs_ = 0;
      remainingKey_ = 0;
      if (!expiryReported_) {
        expiryReported_ = true;
        return TickEvent::Expired;
      }
      return TickEvent::None;
    }

    const uint32_t key = remainingDisplayKey(nowUs);
    if (key != remainingKey_) {
      remainingKey_ = key;
      return TickEvent::RemainingChanged;
    }
    return TickEvent::None;
  }

  constexpr State state() const { return state_; }
  constexpr bool enabled() const { return state_ != State::Disabled; }
  constexpr bool isLocked() const { return state_ == State::Locked; }
  constexpr bool isUnlocked() const {
    return state_ == State::Timed || state_ == State::Unlimited;
  }
  constexpr bool isUnlimited() const { return state_ == State::Unlimited; }

  constexpr uint32_t remainingSeconds(uint64_t nowUs) const {
    if (state_ != State::Timed || nowUs >= deadlineUs_) return 0;
    const uint64_t remainingUs = deadlineUs_ - nowUs;
    const uint64_t seconds = (remainingUs + 999999ULL) / 1000000ULL;
    return seconds > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(seconds);
  }

 private:
  constexpr uint32_t remainingDisplayKey(uint64_t nowUs) const {
    const uint32_t seconds = remainingSeconds(nowUs);
    if (seconds < 60) return 0;
    return (seconds + 59U) / 60U;
  }

  State state_ = State::Disabled;
  uint64_t deadlineUs_ = 0;
  uint32_t remainingKey_ = 0;
  bool expiryReported_ = false;
};

class PinEntry {
 public:
  constexpr void clear() {
    length_ = 0;
    digits_[0] = '\0';
  }

  constexpr bool append(uint8_t digit) {
    if (digit > 9 || length_ >= PIN_LENGTH) return false;
    digits_[length_++] = static_cast<char>('0' + digit);
    digits_[length_] = '\0';
    return true;
  }

  constexpr bool backspace() {
    if (length_ == 0) return false;
    digits_[--length_] = '\0';
    return true;
  }

  constexpr bool complete() const { return length_ == PIN_LENGTH; }
  constexpr uint8_t length() const { return length_; }
  constexpr const char* value() const { return digits_; }

 private:
  char digits_[PIN_LENGTH + 1] = {};
  uint8_t length_ = 0;
};

class AuthLockout {
 public:
  constexpr void fail(uint64_t nowUs) {
    lockedUntilUs_ =
        nowUs + static_cast<uint64_t>(AUTH_LOCKOUT_SECONDS) * 1000000ULL;
  }

  constexpr void clear() { lockedUntilUs_ = 0; }

  constexpr bool active(uint64_t nowUs) const {
    return lockedUntilUs_ != 0 && nowUs < lockedUntilUs_;
  }

  constexpr uint32_t remainingSeconds(uint64_t nowUs) const {
    if (!active(nowUs)) return 0;
    return static_cast<uint32_t>(
        (lockedUntilUs_ - nowUs + 999999ULL) / 1000000ULL);
  }

 private:
  uint64_t lockedUntilUs_ = 0;
};

constexpr bool isSixDigitPin(const char* pin) {
  if (pin == nullptr) return false;
  for (uint8_t i = 0; i < PIN_LENGTH; ++i) {
    if (pin[i] < '0' || pin[i] > '9') return false;
  }
  return pin[PIN_LENGTH] == '\0';
}

}  // namespace KidMode
