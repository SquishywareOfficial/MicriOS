#include "../shared/logic/KidModeLogic.h"

namespace {

constexpr uint64_t SECOND = 1000000ULL;

constexpr bool testTimedSession() {
  KidMode::Service service;
  service.begin(true);
  if (!service.isLocked() || !service.unlockForMinutes(10, 5 * SECOND)) {
    return false;
  }
  if (service.remainingSeconds(5 * SECOND) != 600) return false;
  if (service.tick(605 * SECOND - 1) == KidMode::TickEvent::Expired) {
    return false;
  }
  if (service.tick(605 * SECOND) != KidMode::TickEvent::Expired) return false;
  if (!service.isLocked()) return false;
  return service.tick(606 * SECOND) == KidMode::TickEvent::None;
}

constexpr bool testDurationPresets() {
  constexpr uint16_t durations[] = {10, 20, 30, 60, 120};
  for (uint16_t minutes : durations) {
    KidMode::Service service;
    service.begin(true);
    if (!service.unlockForMinutes(minutes, SECOND)) return false;
    if (service.remainingSeconds(SECOND) != minutes * 60U) return false;
  }
  KidMode::Service invalid;
  invalid.begin(true);
  return !invalid.unlockForMinutes(0, 0) &&
         !invalid.unlockForMinutes(121, 0);
}

constexpr bool testManualAndUnlimitedLock() {
  KidMode::Service service;
  service.begin(true);
  if (!service.unlockUnlimited() || !service.isUnlimited()) return false;
  if (service.tick(UINT64_MAX) != KidMode::TickEvent::None) return false;
  service.lockNow();
  return service.isLocked() && !service.isUnlocked();
}

constexpr bool testAuthenticationLockout() {
  KidMode::AuthLockout lockout;
  lockout.fail(2 * SECOND);
  if (!lockout.active(2 * SECOND) || lockout.remainingSeconds(2 * SECOND) != 30) {
    return false;
  }
  if (!lockout.active(32 * SECOND - 1)) return false;
  if (lockout.active(32 * SECOND) || lockout.remainingSeconds(32 * SECOND) != 0) {
    return false;
  }
  lockout.clear();
  return !lockout.active(0);
}

constexpr bool testPinEntry() {
  KidMode::PinEntry pin;
  for (uint8_t digit = 0; digit < KidMode::PIN_LENGTH; ++digit) {
    if (!pin.append(digit)) return false;
  }
  if (!pin.complete() || pin.append(9)) return false;
  if (!pin.backspace() || pin.complete()) return false;
  if (!pin.append(9) || !pin.complete()) return false;
  return KidMode::isSixDigitPin(pin.value()) &&
         !KidMode::isSixDigitPin("12345") &&
         !KidMode::isSixDigitPin("12345A");
}

constexpr bool testSplashTextEditor() {
  KidMode::SplashTextEditor editor;
  editor.begin("Child's Computer");
  if (editor.empty() || editor.length() != 16) return false;
  if (!editor.append('1') || !editor.backspace()) return false;
  if (editor.append('!')) return false;
  editor.clear();
  for (uint8_t i = 0; i < KidMode::SPLASH_TEXT_MAX_LENGTH; ++i) {
    if (!editor.append('A')) return false;
  }
  return !editor.append('B') &&
         KidMode::validSplashPalette(KidMode::SplashPalette::Rainbow) &&
         !KidMode::validSplashPalette(KidMode::SplashPalette::Count);
}

static_assert(testTimedSession(), "Timed Child Mode sessions must expire once");
static_assert(testDurationPresets(), "Child Mode duration presets must work");
static_assert(testManualAndUnlimitedLock(), "Manual and unlimited modes must work");
static_assert(testAuthenticationLockout(), "PIN lockout must last 30 seconds");
static_assert(testPinEntry(), "PIN entry must require exactly six digits");
static_assert(testSplashTextEditor(), "Splash text editing must remain bounded");

}  // namespace
