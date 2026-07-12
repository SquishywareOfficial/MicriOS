#include "NeedSpeedGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
constexpr uint8_t SHIFT_LED_PIN = 8;
constexpr bool SHIFT_LED_ACTIVE_LOW = true;
constexpr uint8_t MAX_GEAR = 6;
constexpr float IDLE_RPM = 900.0f;
constexpr float REDLINE_RPM = 7000.0f;
constexpr float GOOD_RPM = 5200.0f;
constexpr float PERFECT_LOW_RPM = 5600.0f;
constexpr float PERFECT_HIGH_RPM = 6500.0f;
constexpr float LATE_RPM = 6500.0f;
constexpr float RPM_DROP = 0.58f;
constexpr uint8_t MAX_LEVEL = 5;
Preferences speedPrefs;

const float RPM_CLIMB[MAX_GEAR] = {3300.0f, 2700.0f, 2200.0f, 1750.0f, 1400.0f, 1100.0f};
const float SPEED_GAIN[MAX_GEAR] = {24.0f, 22.0f, 19.0f, 16.0f, 13.0f, 10.0f};
}

NeedSpeedGame::NeedSpeedGame(uint32_t width, uint32_t height, uint32_t left)
    : App("Need Speed", width, height), left_(left) {}

bool NeedSpeedGame::hasCustomOverlay() const {
  return true;
}

void NeedSpeedGame::onAppReset() {
  pinMode(SHIFT_LED_PIN, OUTPUT);
  setShiftLed(false);
  loadBestRun();
  level_ = 1;
  totalRaceMs_ = 0;
  startLevel();
  disqualified_ = false;
  failed_ = false;
}

void NeedSpeedGame::startLevel() {
  raceState_ = RaceState::Countdown;
  message_ = ShiftMessage::None;
  countdownMs_ = 3000;
  raceMs_ = 0;
  messageTimerMs_ = 0;
  levelCompleteTimerMs_ = 0;
  gear_ = 1;
  rpm_ = IDLE_RPM;
  speed_ = 0.0f;
  setShiftLed(false);
}

void NeedSpeedGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  if (messageTimerMs_ > deltaMs) {
    messageTimerMs_ -= deltaMs;
  } else {
    messageTimerMs_ = 0;
    if (message_ != ShiftMessage::FalseStart) {
      message_ = ShiftMessage::None;
    }
  }

  if (raceState_ == RaceState::Countdown) {
    if (input.pressed) {
      disqualified_ = true;
      message_ = ShiftMessage::FalseStart;
      setShiftLed(false);
      endApp();
      return;
    }
    if (countdownMs_ > deltaMs) {
      countdownMs_ -= deltaMs;
    } else {
      countdownMs_ = 0;
      raceState_ = RaceState::LaunchWait;
    }
    return;
  }

  if (raceState_ == RaceState::LaunchWait) {
    if (input.pressed) {
      launch(countdownMs_);
    }
    return;
  }

  if (raceState_ == RaceState::LevelComplete) {
    setShiftLed(false);
    levelCompleteTimerMs_ += deltaMs;
    if (levelCompleteTimerMs_ >= 1200) {
      level_++;
      startLevel();
    }
    return;
  }

  raceMs_ += deltaMs;
  totalRaceMs_ += deltaMs;
  const float deltaSec = static_cast<float>(deltaMs) * 0.001f;
  const uint8_t gearIndex = gear_ - 1;
  const float rpmPenalty = rpm_ > PERFECT_HIGH_RPM ? 0.72f : 1.0f;
  rpm_ += RPM_CLIMB[gearIndex] * levelRpmMultiplier() * deltaSec;
  if (rpm_ > REDLINE_RPM) {
    rpm_ = REDLINE_RPM;
    message_ = ShiftMessage::Limiter;
    messageTimerMs_ = 180;
  }
  speed_ += SPEED_GAIN[gearIndex] * levelSpeedMultiplier() * rpmPenalty * deltaSec;

  if (input.pressed) {
    shift();
  }

  updateShiftLed(millis());

  if (speed_ >= levelFinishSpeed()) {
    setShiftLed(false);
    recordClearedLevel(level_);
    if (level_ >= MAX_LEVEL) {
      endApp();
    } else {
      raceState_ = RaceState::LevelComplete;
      levelCompleteTimerMs_ = 0;
    }
  } else if (gear_ >= MAX_GEAR && rpm_ >= REDLINE_RPM) {
    setShiftLed(false);
    failed_ = true;
    endApp();
  } else if (raceMs_ > levelTargetMs()) {
    setShiftLed(false);
    failed_ = true;
    endApp();
  }
}

void NeedSpeedGame::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  u8g2.setFont(u8g2_font_5x8_tr);

  if (raceState_ == RaceState::Countdown) {
    drawTrafficLights(u8g2);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(left_ + 24, 34, "WAIT");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(left_ + 2, 6);
    u8g2.print("L");
    u8g2.print(level_);
    return;
  }

  if (raceState_ == RaceState::LaunchWait) {
    u8g2.drawStr(left_ + 25, 24, "GO");
    return;
  }

  if (raceState_ == RaceState::LevelComplete) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(left_ + 8, 16, "Level Clear");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(left_ + 20, 28);
    u8g2.print("Next L");
    u8g2.print(level_ + 1);
    return;
  }

  u8g2.setCursor(left_ + 2, 8);
  u8g2.print("L");
  u8g2.print(level_);
  u8g2.print(" ");
  u8g2.print("G");
  u8g2.print(gear_);
  u8g2.setCursor(left_ + 24, 8);
  u8g2.print(static_cast<int>(speed_ + 0.5f));
  u8g2.print("k");
  drawTach(u8g2);

  if (message_ != ShiftMessage::None) {
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(left_ + 3, 38, messageText());
  }
}

void NeedSpeedGame::drawStart(U8G2& u8g2) {
  loadBestRun();
  u8g2.drawFrame(0, 0, width + 2, height);
  if (showStartPromptPage()) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(20, 16, "Press");
    u8g2.drawStr(13, 29, "to Start");
    return;
  }
  if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 10, "Best Run");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(3, 23);
    if (bestLevel_ == 0) {
      u8g2.print("--");
    } else {
      u8g2.print(initials);
      u8g2.print(" L");
      u8g2.print(bestLevel_);
      u8g2.setCursor(3, 32);
      u8g2.print(bestRaceMs_ / 1000);
      u8g2.print(".");
      u8g2.print((bestRaceMs_ / 100) % 10);
      u8g2.print("s");
    }
    return;
  }
  drawCarSplash(u8g2);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, appTitle());
}

void NeedSpeedGame::drawEnd(U8G2& u8g2) {
  setShiftLed(false);
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, disqualified_ ? "False Start" : failed_ ? "Lost" : "Finished");
  u8g2.setCursor(3, 21);
  u8g2.print("Time:");
  u8g2.print(totalRaceMs_ / 1000);
  u8g2.print(".");
  u8g2.print((totalRaceMs_ / 100) % 10);
  u8g2.setCursor(3, 32);
  if (failed_) {
    u8g2.print("Target:");
    u8g2.print(static_cast<int>(levelFinishSpeed() + 0.5f));
    u8g2.print("k ");
    u8g2.print(levelTargetMs() / 1000);
    u8g2.print(".");
    u8g2.print((levelTargetMs() / 100) % 10);
  } else {
    u8g2.print("Level:");
    u8g2.print(level_);
    u8g2.print("/");
    u8g2.print(MAX_LEVEL);
  }
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 39, "Tap retry Hold menu");
}

void NeedSpeedGame::launch(uint32_t lateMs) {
  (void)lateMs;
  raceState_ = RaceState::Racing;
  rpm_ = IDLE_RPM;
  speed_ = 1.0f;
  raceMs_ = 0;
  message_ = ShiftMessage::Good;
  messageTimerMs_ = 350;
}

void NeedSpeedGame::shift() {
  if (gear_ >= MAX_GEAR) {
    return;
  }
  if (rpm_ < GOOD_RPM) {
    message_ = ShiftMessage::Short;
  } else if (rpm_ <= PERFECT_HIGH_RPM && rpm_ >= PERFECT_LOW_RPM) {
    message_ = ShiftMessage::Perfect;
    speed_ += 3.0f;
  } else if (rpm_ <= LATE_RPM) {
    message_ = ShiftMessage::Good;
  } else {
    message_ = ShiftMessage::Late;
  }
  messageTimerMs_ = 450;
  gear_++;
  rpm_ = max(IDLE_RPM, rpm_ * RPM_DROP);
  setShiftLed(false);
}

void NeedSpeedGame::updateShiftLed(uint32_t nowMs) {
  if (raceState_ != RaceState::Racing || gear_ >= MAX_GEAR) {
    setShiftLed(false);
    return;
  }
  if (rpm_ >= LATE_RPM) {
    setShiftLed(((nowMs / 120) % 2) == 0);
  } else {
    setShiftLed(rpm_ >= PERFECT_LOW_RPM);
  }
}

void NeedSpeedGame::setShiftLed(bool on) {
  digitalWrite(SHIFT_LED_PIN, SHIFT_LED_ACTIVE_LOW ? !on : on);
}

void NeedSpeedGame::loadBestRun() {
  if (bestLoaded_) {
    return;
  }
  speedPrefs.begin("needspeed", true);
  bestLevel_ = speedPrefs.getUChar("level", 0);
  bestRaceMs_ = speedPrefs.getUInt("time", 0);
  bestInitials_ = speedPrefs.getUShort("init", PlayerProfile::defaultInitials());
  speedPrefs.end();
  bestLoaded_ = true;
}

void NeedSpeedGame::saveBestRun() {
  bestInitials_ = PlayerProfile::loadInitials();
  speedPrefs.begin("needspeed", false);
  speedPrefs.putUChar("level", bestLevel_);
  speedPrefs.putUInt("time", bestRaceMs_);
  speedPrefs.putUShort("init", bestInitials_);
  speedPrefs.end();
}

void NeedSpeedGame::recordClearedLevel(uint8_t clearedLevel) {
  loadBestRun();
  if (clearedLevel > bestLevel_ ||
      (clearedLevel == bestLevel_ && (bestRaceMs_ == 0 || totalRaceMs_ < bestRaceMs_))) {
    bestLevel_ = clearedLevel;
    bestRaceMs_ = totalRaceMs_;
    saveBestRun();
  }
}

float NeedSpeedGame::levelFinishSpeed() const {
  return 120.0f + static_cast<float>(level_ - 1) * 28.0f;
}

uint16_t NeedSpeedGame::levelTargetMs() const {
  return 14500 - static_cast<uint16_t>(level_ - 1) * 1100;
}

float NeedSpeedGame::levelRpmMultiplier() const {
  return 1.0f + static_cast<float>(level_ - 1) * 0.08f;
}

float NeedSpeedGame::levelSpeedMultiplier() const {
  return 1.0f + static_cast<float>(level_ - 1) * 0.04f;
}

void NeedSpeedGame::drawTrafficLights(U8G2& u8g2) {
  const uint8_t lightsOn = (countdownMs_ + 999) / 1000;
  const int y = 16;
  for (uint8_t i = 0; i < 3; i++) {
    const int x = left_ + 22 + (i * 10);
    u8g2.drawCircle(x, y, 4);
    if (i < lightsOn) {
      u8g2.drawDisc(x, y, 2);
    }
  }
}

void NeedSpeedGame::drawCarSplash(U8G2& u8g2) {
  u8g2.drawLine(8, 25, 26, 25);
  u8g2.drawLine(10, 22, 18, 18);
  u8g2.drawLine(18, 18, 25, 22);
  u8g2.drawLine(7, 25, 5, 28);
  u8g2.drawLine(26, 25, 29, 28);
  u8g2.drawLine(5, 28, 29, 28);
  u8g2.drawCircle(10, 29, 2);
  u8g2.drawCircle(24, 29, 2);

  u8g2.drawLine(41, 27, 62, 27);
  u8g2.drawLine(44, 24, 52, 20);
  u8g2.drawLine(52, 20, 61, 24);
  u8g2.drawLine(39, 27, 37, 30);
  u8g2.drawLine(62, 27, 65, 30);
  u8g2.drawLine(37, 30, 65, 30);
  u8g2.drawCircle(44, 31, 2);
  u8g2.drawCircle(59, 31, 2);
}

void NeedSpeedGame::drawTach(U8G2& u8g2) {
  const int cx = left_ + 36;
  const int cy = 27;
  const int radius = 14;
  const float startAngle = 2.35f;
  const float sweepAngle = 3.25f;
  u8g2.drawCircle(cx, cy, radius);

  const float lowAngle = startAngle + (PERFECT_LOW_RPM / REDLINE_RPM) * sweepAngle;
  const float highAngle = startAngle + (PERFECT_HIGH_RPM / REDLINE_RPM) * sweepAngle;
  for (float angle = lowAngle; angle <= highAngle; angle += 0.10f) {
    u8g2.drawLine(
        cx + static_cast<int>(cosf(angle) * 9),
        cy + static_cast<int>(sinf(angle) * 9),
        cx + static_cast<int>(cosf(angle) * 13),
        cy + static_cast<int>(sinf(angle) * 13));
  }

  for (uint8_t i = 0; i <= 7; i++) {
    const float angle = startAngle + (static_cast<float>(i) * sweepAngle / 7.0f);
    const int x1 = cx + static_cast<int>(cosf(angle) * (radius - 3));
    const int y1 = cy + static_cast<int>(sinf(angle) * (radius - 3));
    const int x2 = cx + static_cast<int>(cosf(angle) * radius);
    const int y2 = cy + static_cast<int>(sinf(angle) * radius);
    u8g2.drawLine(x1, y1, x2, y2);
  }

  u8g2.drawLine(
      cx + static_cast<int>(cosf(lowAngle) * 10),
      cy + static_cast<int>(sinf(lowAngle) * 10),
      cx + static_cast<int>(cosf(lowAngle) * 15),
      cy + static_cast<int>(sinf(lowAngle) * 15));
  u8g2.drawLine(
      cx + static_cast<int>(cosf(highAngle) * 10),
      cy + static_cast<int>(sinf(highAngle) * 10),
      cx + static_cast<int>(cosf(highAngle) * 15),
      cy + static_cast<int>(sinf(highAngle) * 15));

  const float rpmRatio = min(1.0f, rpm_ / REDLINE_RPM);
  const float needleAngle = startAngle + rpmRatio * sweepAngle;
  u8g2.drawLine(
      cx,
      cy,
      cx + static_cast<int>(cosf(needleAngle) * 11),
      cy + static_cast<int>(sinf(needleAngle) * 11));
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 3, 31);
  u8g2.print(static_cast<int>(rpm_ / 100.0f));
  u8g2.print("00");
}

const char* NeedSpeedGame::messageText() const {
  switch (message_) {
    case ShiftMessage::Short:
      return "Short shift";
    case ShiftMessage::Good:
      return "Good";
    case ShiftMessage::Perfect:
      return "Perfect";
    case ShiftMessage::Late:
      return "Late shift";
    case ShiftMessage::Limiter:
      return "Limiter";
    case ShiftMessage::FalseStart:
      return "Disqualified";
    case ShiftMessage::None:
    default:
      return "";
  }
}
