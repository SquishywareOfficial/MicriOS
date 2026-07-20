#include "NeedSpeedGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../CydFramebuffer.h"
#include "../../CydHardware.h"
#include "../../CydUi.h"

namespace {
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

template <typename Canvas>
void drawTrafficLightsOn(Canvas& canvas, uint16_t countdownMs) {
  const uint8_t lightsOn = (countdownMs + 999) / 1000;
  const int y = 92;
  for (uint8_t i = 0; i < 3; i++) {
    const int x = 112 + (i * 48);
    canvas.drawCircle(x, y, 18, TFT_LIGHTGREY);
    if (i < lightsOn) {
      canvas.fillCircle(x, y, 13, TFT_RED);
    }
  }
}

template <typename Canvas>
void drawTachOn(Canvas& canvas, float rpm) {
  const int cx = 218;
  const int cy = 111;
  const int radius = 65;
  const float startAngle = 2.35f;
  const float sweepAngle = 3.25f;
  canvas.drawCircle(cx, cy, radius, TFT_LIGHTGREY);

  const float lowAngle = startAngle + (PERFECT_LOW_RPM / REDLINE_RPM) * sweepAngle;
  const float highAngle = startAngle + (PERFECT_HIGH_RPM / REDLINE_RPM) * sweepAngle;
  for (float angle = lowAngle; angle <= highAngle; angle += 0.10f) {
    canvas.drawLine(cx + static_cast<int>(cosf(angle) * 44), cy + static_cast<int>(sinf(angle) * 44),
                    cx + static_cast<int>(cosf(angle) * 62), cy + static_cast<int>(sinf(angle) * 62), TFT_GREEN);
  }

  for (uint8_t i = 0; i <= 7; i++) {
    const float angle = startAngle + (static_cast<float>(i) * sweepAngle / 7.0f);
    const int x1 = cx + static_cast<int>(cosf(angle) * (radius - 8));
    const int y1 = cy + static_cast<int>(sinf(angle) * (radius - 3));
    const int x2 = cx + static_cast<int>(cosf(angle) * radius);
    const int y2 = cy + static_cast<int>(sinf(angle) * radius);
    canvas.drawLine(x1, y1, x2, y2, TFT_LIGHTGREY);
  }

  const float rpmRatio = min(1.0f, rpm / REDLINE_RPM);
  const float needleAngle = startAngle + rpmRatio * sweepAngle;
  canvas.drawLine(cx, cy, cx + static_cast<int>(cosf(needleAngle) * 52), cy + static_cast<int>(sinf(needleAngle) * 52),
                  rpm >= LATE_RPM ? TFT_RED : TFT_YELLOW);
  canvas.fillCircle(cx, cy, 6, TFT_WHITE);

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(cx - 23, cy + 38);
  canvas.print(static_cast<int>(rpm / 100.0f));
  canvas.print("00");
}
}

NeedSpeedGame::NeedSpeedGame(uint32_t width, uint32_t height)
    : App("Need Speed", width, height) {}

bool NeedSpeedGame::hasCustomOverlay() const {
  return true;
}

void NeedSpeedGame::onAppReset() {
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

void NeedSpeedGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)b2;
  if (messageTimerMs_ > deltaMs) {
    messageTimerMs_ -= deltaMs;
  } else {
    messageTimerMs_ = 0;
    if (message_ != ShiftMessage::FalseStart) {
      message_ = ShiftMessage::None;
    }
  }

  if (raceState_ == RaceState::Countdown) {
    if (b1.pressed) {
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
    if (b1.pressed) {
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

  if (b1.pressed) {
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

void NeedSpeedGame::drawRunning(TFT_eSPI& tft) {
  CydFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    CydUi::clear(canvas);
    CydUi::header(canvas, "Need Speed", TFT_RED, (String("L") + String(level_)).c_str());
    if (raceState_ == RaceState::Countdown) {
      drawTrafficLightsOn(canvas, countdownMs_);
      CydUi::centered(canvas, "WAIT", 132, 3, TFT_YELLOW);
      CydUi::footer(canvas, "Launch when lights are out");
      return;
    }

    if (raceState_ == RaceState::LaunchWait) {
      drawTrafficLightsOn(canvas, countdownMs_);
      CydUi::centered(canvas, "GO", 127, 5, TFT_GREEN);
      CydUi::footer(canvas, "Tap to launch");
      return;
    }

    if (raceState_ == RaceState::LevelComplete) {
      CydUi::centered(canvas, "Level Clear", 69, 4, TFT_GREEN);
      CydUi::centered(canvas, "Next L" + String(level_ + 1), 125, 2, TFT_LIGHTGREY);
      CydUi::footer(canvas, "Next race...");
      return;
    }

    drawTachOn(canvas, rpm_);
    CydUi::labelValue(canvas, 51, "Gear", String(gear_), TFT_CYAN);
    CydUi::labelValue(canvas, 85, "Speed", String(static_cast<int>(speed_ + 0.5f)) + " km/h", TFT_GREEN);
    CydUi::labelValue(canvas, 119, "Time", String(raceMs_ / 1000) + "." + String((raceMs_ / 100) % 10) + "s", TFT_YELLOW);

    if (message_ != ShiftMessage::None) {
      CydUi::footer(canvas, messageText());
    } else {
      CydUi::footer(canvas, "Tap to shift in green band");
    }
  });
}

void NeedSpeedGame::drawStart(TFT_eSPI& tft) { CydUi::clear(tft);
  loadBestRun();
  CydUi::clear(tft);
  if (showStartPromptPage()) {
    CydUi::header(tft, "Need Speed", TFT_RED);
    CydUi::centered(tft, "Press to Start", 56, 2, TFT_WHITE);
    CydUi::footer(tft, "Wait for GO, then shift");
    return;
  }
  if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    CydUi::header(tft, "Best Run", TFT_RED);
    String level = bestLevel_ == 0 ? "--" : String(initials) + " L" + String(bestLevel_);
    String time = bestLevel_ == 0 ? "--" : String(bestRaceMs_ / 1000) + "." + String((bestRaceMs_ / 100) % 10) + "s";
    CydUi::labelValue(tft, 52, "Level", level, TFT_YELLOW);
    CydUi::labelValue(tft, 80, "Time", time, TFT_GREEN);
    CydUi::footer(tft, "Best level and time");
    return;
  }
  CydUi::header(tft, "Need Speed", TFT_RED);
  drawCarSplash(tft);
  CydUi::footer(tft, "Street shift challenge");
}

void NeedSpeedGame::drawEnd(TFT_eSPI& tft) { CydUi::clear(tft);
  setShiftLed(false);
  CydUi::clear(tft);
  CydUi::header(tft, disqualified_ ? "False Start" : failed_ ? "Lost" : "Finished", failed_ || disqualified_ ? TFT_RED : TFT_GREEN);
  CydUi::labelValue(tft, 45, "Time", String(totalRaceMs_ / 1000) + "." + String((totalRaceMs_ / 100) % 10) + "s", TFT_YELLOW);
  if (failed_) {
    CydUi::labelValue(tft, 72, "Target", String(static_cast<int>(levelFinishSpeed() + 0.5f)) + "k " + String(levelTargetMs() / 1000) + "." + String((levelTargetMs() / 100) % 10) + "s", TFT_RED);
  } else {
    CydUi::labelValue(tft, 72, "Level", String(level_) + "/" + String(MAX_LEVEL), TFT_GREEN);
  }
  CydUi::footer(tft, "Tap Play Again");
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
  CydHardware::setRgb(0, on ? 255 : 0, 0);
}

void NeedSpeedGame::onAppExit() { setShiftLed(false); }

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

void NeedSpeedGame::drawTrafficLights(TFT_eSPI& tft) {
  drawTrafficLightsOn(tft, countdownMs_);
}

void NeedSpeedGame::drawCarSplash(TFT_eSPI& tft) {
  tft.drawFastHLine(0, 168, width, TFT_DARKGREY);
  tft.fillRoundRect(26, 113, 112, 28, 7, TFT_BLUE);
  tft.drawLine(42, 113, 68, 83, TFT_CYAN);
  tft.drawLine(68, 83, 114, 113, TFT_CYAN);
  tft.fillRect(49, 119, 67, 13, TFT_CYAN);
  tft.fillCircle(52, 145, 11, TFT_BLACK);
  tft.fillCircle(116, 145, 11, TFT_BLACK);
  tft.fillRoundRect(176, 103, 118, 31, 7, TFT_RED);
  tft.drawLine(193, 103, 220, 73, TFT_ORANGE);
  tft.drawLine(220, 73, 271, 103, TFT_ORANGE);
  tft.fillRect(200, 111, 70, 14, TFT_ORANGE);
  tft.fillCircle(202, 139, 11, TFT_BLACK);
  tft.fillCircle(274, 139, 11, TFT_BLACK);
}

void NeedSpeedGame::drawTach(TFT_eSPI& tft) {
  drawTachOn(tft, rpm_);
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
