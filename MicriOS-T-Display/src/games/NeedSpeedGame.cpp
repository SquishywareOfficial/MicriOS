#include "NeedSpeedGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
constexpr uint8_t SHIFT_LED_PIN = 8;
constexpr bool SHIFT_LED_ACTIVE_LOW = true;
constexpr bool SHIFT_LED_AVAILABLE = false;
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
  const int y = 57;
  for (uint8_t i = 0; i < 3; i++) {
    const int x = 84 + (i * 36);
    canvas.drawCircle(x, y, 14, TFT_LIGHTGREY);
    if (i < lightsOn) {
      canvas.fillCircle(x, y, 10, TFT_RED);
    }
  }
}

template <typename Canvas>
void drawTachOn(Canvas& canvas, float rpm) {
  const int cx = 158;
  const int cy = 76;
  const int radius = 43;
  const float startAngle = 2.35f;
  const float sweepAngle = 3.25f;
  canvas.drawCircle(cx, cy, radius, TFT_LIGHTGREY);

  const float lowAngle = startAngle + (PERFECT_LOW_RPM / REDLINE_RPM) * sweepAngle;
  const float highAngle = startAngle + (PERFECT_HIGH_RPM / REDLINE_RPM) * sweepAngle;
  for (float angle = lowAngle; angle <= highAngle; angle += 0.10f) {
    canvas.drawLine(cx + static_cast<int>(cosf(angle) * 28), cy + static_cast<int>(sinf(angle) * 28),
                    cx + static_cast<int>(cosf(angle) * 41), cy + static_cast<int>(sinf(angle) * 41), TFT_GREEN);
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
  canvas.drawLine(cx, cy, cx + static_cast<int>(cosf(needleAngle) * 34), cy + static_cast<int>(sinf(needleAngle) * 34),
                  rpm >= LATE_RPM ? TFT_RED : TFT_YELLOW);
  canvas.fillCircle(cx, cy, 4, TFT_WHITE);

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(cx - 23, cy + 25);
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
  if (SHIFT_LED_AVAILABLE) {
    pinMode(SHIFT_LED_PIN, OUTPUT);
  }
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
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    TDisplayUi::clear(canvas);
    TDisplayUi::header(canvas, "Need Speed", TFT_RED, (String("L") + String(level_)).c_str());
    if (raceState_ == RaceState::Countdown) {
      drawTrafficLightsOn(canvas, countdownMs_);
      TDisplayUi::centered(canvas, "WAIT", 84, 3, TFT_YELLOW);
      TDisplayUi::footer(canvas, "Launch when lights are out");
      return;
    }

    if (raceState_ == RaceState::LaunchWait) {
      drawTrafficLightsOn(canvas, countdownMs_);
      TDisplayUi::centered(canvas, "GO", 79, 5, TFT_GREEN);
      TDisplayUi::footer(canvas, "B1 launch");
      return;
    }

    if (raceState_ == RaceState::LevelComplete) {
      TDisplayUi::centered(canvas, "Level Clear", 48, 3, TFT_GREEN);
      TDisplayUi::centered(canvas, "Next L" + String(level_ + 1), 86, 2, TFT_LIGHTGREY);
      TDisplayUi::footer(canvas, "Next race...");
      return;
    }

    drawTachOn(canvas, rpm_);
    TDisplayUi::labelValue(canvas, 34, "Gear", String(gear_), TFT_CYAN);
    TDisplayUi::labelValue(canvas, 57, "Speed", String(static_cast<int>(speed_ + 0.5f)) + " km/h", TFT_GREEN);
    TDisplayUi::labelValue(canvas, 80, "Time", String(raceMs_ / 1000) + "." + String((raceMs_ / 100) % 10) + "s", TFT_YELLOW);

    if (message_ != ShiftMessage::None) {
      TDisplayUi::footer(canvas, messageText());
    } else {
      TDisplayUi::footer(canvas, "B1 shift in green band");
    }
  });
}

void NeedSpeedGame::drawStart(TFT_eSPI& tft) { tft.fillScreen(TFT_BLACK);
  loadBestRun();
  TDisplayUi::clear(tft);
  if (showStartPromptPage()) {
    TDisplayUi::header(tft, "Need Speed", TFT_RED);
    TDisplayUi::centered(tft, "Press to Start", 56, 2, TFT_WHITE);
    TDisplayUi::footer(tft, "Wait for GO, then shift");
    return;
  }
  if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    TDisplayUi::header(tft, "Best Run", TFT_RED);
    String level = bestLevel_ == 0 ? "--" : String(initials) + " L" + String(bestLevel_);
    String time = bestLevel_ == 0 ? "--" : String(bestRaceMs_ / 1000) + "." + String((bestRaceMs_ / 100) % 10) + "s";
    TDisplayUi::labelValue(tft, 52, "Level", level, TFT_YELLOW);
    TDisplayUi::labelValue(tft, 80, "Time", time, TFT_GREEN);
    TDisplayUi::footer(tft, "Best level and time");
    return;
  }
  TDisplayUi::header(tft, "Need Speed", TFT_RED);
  drawCarSplash(tft);
  TDisplayUi::footer(tft, "Street shift challenge");
}

void NeedSpeedGame::drawEnd(TFT_eSPI& tft) { tft.fillScreen(TFT_BLACK);
  setShiftLed(false);
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, disqualified_ ? "False Start" : failed_ ? "Lost" : "Finished", failed_ || disqualified_ ? TFT_RED : TFT_GREEN);
  TDisplayUi::labelValue(tft, 45, "Time", String(totalRaceMs_ / 1000) + "." + String((totalRaceMs_ / 100) % 10) + "s", TFT_YELLOW);
  if (failed_) {
    TDisplayUi::labelValue(tft, 72, "Target", String(static_cast<int>(levelFinishSpeed() + 0.5f)) + "k " + String(levelTargetMs() / 1000) + "." + String((levelTargetMs() / 100) % 10) + "s", TFT_RED);
  } else {
    TDisplayUi::labelValue(tft, 72, "Level", String(level_) + "/" + String(MAX_LEVEL), TFT_GREEN);
  }
  TDisplayUi::footer(tft, "B1 retry / B1 hold menu");
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
  if (!SHIFT_LED_AVAILABLE) {
    return;
  }
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

void NeedSpeedGame::drawTrafficLights(TFT_eSPI& tft) {
  drawTrafficLightsOn(tft, countdownMs_);
}

void NeedSpeedGame::drawCarSplash(TFT_eSPI& tft) {
  tft.drawFastHLine(0, 106, width, TFT_DARKGREY);
  tft.fillRoundRect(34, 75, 70, 17, 5, TFT_BLUE);
  tft.drawLine(44, 75, 60, 60, TFT_CYAN);
  tft.drawLine(60, 60, 88, 75, TFT_CYAN);
  tft.fillRect(48, 78, 42, 8, TFT_CYAN);
  tft.fillCircle(50, 94, 7, TFT_BLACK);
  tft.fillCircle(88, 94, 7, TFT_BLACK);
  tft.fillRoundRect(134, 69, 78, 19, 5, TFT_RED);
  tft.drawLine(145, 69, 163, 52, TFT_ORANGE);
  tft.drawLine(163, 52, 198, 69, TFT_ORANGE);
  tft.fillRect(150, 73, 46, 9, TFT_ORANGE);
  tft.fillCircle(152, 91, 7, TFT_BLACK);
  tft.fillCircle(198, 91, 7, TFT_BLACK);
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
