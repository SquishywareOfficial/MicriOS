#include "MicriFieldGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
constexpr uint8_t FIELD_LED_PIN = 8;
constexpr bool FIELD_LED_ACTIVE_LOW = true;
constexpr bool FIELD_LED_AVAILABLE = false;
constexpr uint8_t EVENT_COUNT = 6;
constexpr uint16_t EXTRA_LIFE_POINTS = 1000;
constexpr uint16_t RESULT_LOCK_MS = 650;
constexpr uint8_t STARTING_ATTEMPT_BANK = 5;
constexpr uint16_t THROW_ANIM_MS = 1400;
Preferences fieldPrefs;

const char* const EVENT_NAMES[EVENT_COUNT] = {
    "100m Dash",
    "Hurdles",
    "Long Jump",
    "Hammer",
    "Javelin",
    "High Jump",
};

int absInt(int value) {
  return value < 0 ? -value : value;
}

float absFloat(float value) {
  return value < 0.0f ? -value : value;
}

float clampFloat(float value, float low, float high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

String formatEventValue(uint8_t eventIndex, uint16_t value) {
  if (eventIndex <= 1) {
    return String(value / 1000) + "." + String((value / 100) % 10) + "s";
  }
  return String(value / 100) + "." + String((value / 10) % 10) + "m";
}

void migrateLegacyFieldPrefs() {
  Preferences current;
  current.begin("micrifld", true);
  bool hasCurrent = current.isKey("total") || current.isKey("init");
  for (uint8_t i = 0; i < EVENT_COUNT && !hasCurrent; i++) {
    char key[5];
    snprintf(key, sizeof(key), "ev%u", i);
    hasCurrent = current.isKey(key);
  }
  current.end();
  if (hasCurrent) {
    return;
  }

  Preferences legacy;
  legacy.begin("femtofld", true);
  bool hasLegacy = legacy.isKey("total") || legacy.isKey("init");
  for (uint8_t i = 0; i < EVENT_COUNT && !hasLegacy; i++) {
    char evKey[5];
    char eKey[4];
    snprintf(evKey, sizeof(evKey), "ev%u", i);
    snprintf(eKey, sizeof(eKey), "e%u", i);
    hasLegacy = legacy.isKey(evKey) || legacy.isKey(eKey);
  }
  if (!hasLegacy) {
    legacy.end();
    return;
  }

  const uint32_t total = legacy.getUInt("total", 0);
  const uint16_t initials = legacy.getUShort("init", PlayerProfile::defaultInitials());
  uint16_t eventScores[EVENT_COUNT] = {};
  for (uint8_t i = 0; i < EVENT_COUNT; i++) {
    char evKey[5];
    char eKey[4];
    snprintf(evKey, sizeof(evKey), "ev%u", i);
    snprintf(eKey, sizeof(eKey), "e%u", i);
    eventScores[i] = legacy.isKey(evKey) ? legacy.getUShort(evKey, 0) : legacy.getUShort(eKey, 0);
  }
  legacy.end();

  Preferences migrated;
  migrated.begin("micrifld", false);
  migrated.putUInt("total", total);
  migrated.putUShort("init", initials);
  for (uint8_t i = 0; i < EVENT_COUNT; i++) {
    char key[5];
    snprintf(key, sizeof(key), "ev%u", i);
    migrated.putUShort(key, eventScores[i]);
  }
  migrated.end();
}
}

MicriFieldGame::MicriFieldGame(uint32_t width, uint32_t height)
    : App("Micri Field", width, height) {}

bool MicriFieldGame::hasCustomOverlay() const {
  return true;
}

void MicriFieldGame::onAppReset() {
  if (FIELD_LED_AVAILABLE) {
    pinMode(FIELD_LED_PIN, OUTPUT);
  }
  setFieldLed(false);
  loadRecords();
  eventIndex_ = 0;
  round_ = 1;
  attemptsRemaining_ = STARTING_ATTEMPT_BANK;
  pressureMode_ = false;
  roundHadZero_ = false;
  totalScore_ = 0;
  nextExtraLifeAt_ = EXTRA_LIFE_POINTS;
  startEventIntro();
}

void MicriFieldGame::startEventIntro() {
  state_ = FieldState::EventIntro;
  stateTimerMs_ = 0;
  bestAttemptScore_ = 0;
  bestAttemptValue_ = 0;
  qualified_ = false;
  extraLifeEarned_ = false;
  throwAnimMs_ = 0;
  throwSignedDistanceCm_ = 0;
  messageTimerMs_ = 0;
  setFieldLed(false);
}

void MicriFieldGame::startAttempt() {
  bestAttemptScore_ = 0;
  bestAttemptValue_ = 0;
  extraLifeEarned_ = false;
  stateTimerMs_ = 0;

  switch (eventIndex_) {
    case 0:
      startCueSequence(CueMode::Dash, 10, max<uint16_t>(360, 510 - round_ * 14));
      break;
    case 1:
      hurdleIndex_ = 0;
      hurdlesCleared_ = 0;
      hurdleScrollMs_ = 0;
      hurdleRunnerY_ = 0.0f;
      hurdleVy_ = 0.0f;
      hurdleX_ = static_cast<float>(width + 8);
      hurdleSpeed_ = 76.0f + static_cast<float>(round_) * 4.0f;
      hurdleScored_ = false;
      hurdleHit_ = false;
      hurdleJumped_ = false;
      runQuality_ = 0;
      cueElapsedMs_ = 0;
      cueTargetMs_ = max<uint16_t>(360, 540 - round_ * 16);
      cueBaseTargetMs_ = cueTargetMs_;
      state_ = FieldState::HurdleCue;
      setFieldLed(false);
      break;
    case 2:
      startCueSequence(CueMode::LongRun, 7, max<uint16_t>(350, 500 - round_ * 12));
      break;
    case 3:
      directionDeg_ = 180.0f;
      directionQuality_ = 0;
      angleQuality_ = 0;
      powerQuality_ = 0;
      state_ = FieldState::DirectionSelect;
      setFieldLed(false);
      break;
    case 4:
      startCueSequence(CueMode::JavelinRun, 6, max<uint16_t>(350, 490 - round_ * 12));
      break;
    case 5:
    default:
      holding_ = false;
      highHoldMs_ = 0;
      highAngleQuality_ = 0;
      state_ = FieldState::HighJumpHold;
      setFieldLed(false);
      break;
  }
}

void MicriFieldGame::startCueSequence(CueMode mode, uint8_t total, uint16_t targetMs) {
  cueMode_ = mode;
  cueTotal_ = total;
  cueDone_ = 0;
  cueQualitySum_ = 0;
  cueElapsedMs_ = 0;
  cueBaseTargetMs_ = targetMs;
  cueTargetMs_ = targetMs;
  lastCueErrorMs_ = 0;
  lastQuality_ = 0;
  messageTimerMs_ = 0;
  state_ = FieldState::CueSequence;
  stateTimerMs_ = 0;
  setFieldLed(false);
}

void MicriFieldGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  const uint16_t cappedDelta = static_cast<uint16_t>(min<uint32_t>(deltaMs, 180));
  stateTimerMs_ += cappedDelta;
  if (messageTimerMs_ > cappedDelta) {
    messageTimerMs_ -= cappedDelta;
  } else {
    messageTimerMs_ = 0;
  }

  if (state_ == FieldState::EventIntro) {
    setFieldLed(false);
    if (stateTimerMs_ >= 450 && b1.click) {
      startAttempt();
    }
    return;
  }

  if (state_ == FieldState::Result) {
    setFieldLed(false);
    handleResultInput(b1, b2);
    return;
  }

  if (state_ == FieldState::ThrowAnim) {
    throwAnimMs_ += cappedDelta;
    setFieldLed(false);
    if (throwAnimMs_ >= THROW_ANIM_MS) {
      state_ = FieldState::Result;
      stateTimerMs_ = 0;
    }
    return;
  }

  if (state_ == FieldState::CueSequence) {
    cueElapsedMs_ += cappedDelta;
    updateCueLed();
    bool accepted = false;
    int16_t error = 0;
    if (b1.pressed) {
      error = static_cast<int16_t>(cueElapsedMs_) - static_cast<int16_t>(cueTargetMs_);
      accepted = true;
    } else if (cueElapsedMs_ > cueTargetMs_ + 430) {
      error = 430;
      accepted = true;
    }

    if (accepted) {
      const uint8_t quality = timingQuality(error);
      cueQualitySum_ += quality;
      cueDone_++;
      lastCueErrorMs_ = error;
      lastQuality_ = quality;
      if (error < 0) {
        messageTimerMs_ = 650;
      }
      if (cueDone_ >= cueTotal_) {
        setFieldLed(false);
        finishCueSequence();
      } else {
        const int16_t jitter = static_cast<int16_t>(random(0, 110)) - 35;
        cueElapsedMs_ = 0;
        cueTargetMs_ = static_cast<uint16_t>(max<int>(300, static_cast<int>(cueBaseTargetMs_) + jitter));
      }
    }
    return;
  }

  if (state_ == FieldState::HurdleCue) {
    const float dt = static_cast<float>(cappedDelta) * 0.001f;
    if ((b1.click || b1.pressed) && hurdleRunnerY_ <= 0.1f) {
      hurdleVy_ = 46.0f;
      hurdleJumped_ = true;
    }

    hurdleRunnerY_ += hurdleVy_ * dt;
    hurdleVy_ -= 88.0f * dt;
    if (hurdleRunnerY_ < 0.0f) {
      hurdleRunnerY_ = 0.0f;
      hurdleVy_ = 0.0f;
    }

    hurdleX_ -= hurdleSpeed_ * dt;
    const float runnerX = 18.0f;
    if (!hurdleScored_ && hurdleX_ <= runnerX + 2.0f) {
      const bool cleared = hurdleRunnerY_ >= 7.0f;
      if (cleared) {
        hurdlesCleared_++;
        runQuality_ += min<uint8_t>(100, 62 + static_cast<uint8_t>(hurdleRunnerY_ * 3.0f));
        hurdleHit_ = false;
      } else {
        runQuality_ += 10;
        hurdleHit_ = true;
      }
      hurdleIndex_++;
      hurdleScored_ = true;
    }

    if (hurdleX_ < -12.0f) {
      if (hurdleIndex_ >= 8) {
        const uint8_t avg = runQuality_ / 8;
        const uint16_t timeMs = 17200 - static_cast<uint16_t>(hurdlesCleared_) * 520 -
                                static_cast<uint16_t>(avg) * 12 - static_cast<uint16_t>(round_) * 80;
        const bool qualified = hurdlesCleared_ >= 6 && timeMs <= qualifyingTarget();
        const uint16_t score = qualified ? static_cast<uint16_t>(80 + hurdlesCleared_ * 12 + max<int>(0, qualifyingTarget() - timeMs) / 35 + avg / 2) : avg / 3;
        finishAttempt(qualified, score, timeMs, ResultUnit::Time);
      } else {
        hurdleX_ = static_cast<float>(width + random(8, 24));
        hurdleSpeed_ += 0.8f;
        hurdleScored_ = false;
        hurdleHit_ = false;
      }
    }
    return;
  }

  if (state_ == FieldState::DirectionSelect) {
    directionDeg_ += static_cast<float>(cappedDelta) * (0.16f + static_cast<float>(round_) * 0.012f);
    while (directionDeg_ >= 360.0f) directionDeg_ -= 360.0f;
    if (b1.pressed) {
      directionQuality_ = angleQuality(directionDeg_, 0.0f, 88.0f);
      selectorAngle_ = 20.0f;
      selectorDir_ = 1;
      state_ = FieldState::AngleSelect;
      stateTimerMs_ = 0;
    }
    return;
  }

  if (state_ == FieldState::AngleSelect) {
    selectorAngle_ += static_cast<float>(selectorDir_) * static_cast<float>(cappedDelta) * (0.060f + static_cast<float>(round_) * 0.004f);
    if (selectorAngle_ > 65.0f) {
      selectorAngle_ = 65.0f;
      selectorDir_ = -1;
    } else if (selectorAngle_ < 20.0f) {
      selectorAngle_ = 20.0f;
      selectorDir_ = 1;
    }
    if (!b1.pressed) {
      return;
    }

    if (eventIndex_ == 2) {
      angleQuality_ = angleQuality(selectorAngle_, 42.0f, 24.0f);
      const uint16_t distanceCm = 235 + static_cast<uint16_t>(runQuality_) * 5 + static_cast<uint16_t>(angleQuality_) * 2;
      const bool qualified = distanceCm >= qualifyingTarget();
      const uint16_t score = qualified ? static_cast<uint16_t>(70 + max<int>(0, distanceCm - qualifyingTarget()) / 7 + runQuality_ / 2 + angleQuality_ / 3) : (runQuality_ + angleQuality_) / 5;
      finishAttempt(qualified, score, distanceCm, ResultUnit::Distance);
    } else if (eventIndex_ == 3) {
      angleQuality_ = angleQuality(selectorAngle_, 45.0f, 26.0f);
      startCueSequence(CueMode::HammerPower, 5, max<uint16_t>(330, 430 - round_ * 10));
    } else if (eventIndex_ == 4) {
      selectorAngle_ = 15.0f;
      holding_ = false;
      state_ = FieldState::JavelinHoldAngle;
      stateTimerMs_ = 0;
    } else {
      angleQuality_ = angleQuality(selectorAngle_, 43.0f, 25.0f);
      const uint16_t distanceCm = 1700 + static_cast<uint16_t>(runQuality_) * 28 + static_cast<uint16_t>(angleQuality_) * 19;
      const bool qualified = distanceCm >= qualifyingTarget();
      const uint16_t score = qualified ? static_cast<uint16_t>(75 + max<int>(0, distanceCm - qualifyingTarget()) / 45 + runQuality_ / 2 + angleQuality_ / 3) : (runQuality_ + angleQuality_) / 5;
      finishAttempt(qualified, score, distanceCm, ResultUnit::Distance);
    }
    return;
  }

  if (state_ == FieldState::JavelinHoldAngle) {
    if (!holding_ && b1.pressed) {
      holding_ = true;
      selectorAngle_ = 15.0f;
    } else if (holding_ && b1.down) {
      selectorAngle_ += static_cast<float>(cappedDelta) * (0.072f + static_cast<float>(round_) * 0.004f);
      if (selectorAngle_ > 70.0f) {
        selectorAngle_ = 70.0f;
      }
    }

    if (holding_ && (b1.released || selectorAngle_ >= 70.0f)) {
      holding_ = false;
      angleQuality_ = angleQuality(selectorAngle_, 43.0f, 25.0f);
      const uint16_t distanceCm = 1700 + static_cast<uint16_t>(runQuality_) * 28 + static_cast<uint16_t>(angleQuality_) * 19;
      const bool qualified = distanceCm >= qualifyingTarget();
      const uint16_t score = qualified ? static_cast<uint16_t>(75 + max<int>(0, distanceCm - qualifyingTarget()) / 45 + runQuality_ / 2 + angleQuality_ / 3) : 0;
      finishAttempt(qualified, score, distanceCm, ResultUnit::Distance);
    }
    return;
  }

  if (state_ == FieldState::HighJumpHold) {
    if (!holding_ && b1.pressed) {
      holding_ = true;
      highHoldMs_ = 0;
    } else if (holding_ && b1.down && highHoldMs_ < 760) {
      highHoldMs_ += cappedDelta;
    }

    if (holding_ && (b1.released || highHoldMs_ >= 760)) {
      const float selectedAngle = clampFloat(18.0f + static_cast<float>(highHoldMs_) * 0.095f, 18.0f, 72.0f);
      highAngleQuality_ = angleQuality(selectedAngle, 42.0f, 25.0f);
      holding_ = false;
      startCueSequence(CueMode::HighBonus, 3, max<uint16_t>(360, 510 - round_ * 15));
    }
    return;
  }
}

void MicriFieldGame::finishCueSequence() {
  const uint8_t avg = cueTotal_ == 0 ? 0 : static_cast<uint8_t>(cueQualitySum_ / cueTotal_);
  switch (cueMode_) {
    case CueMode::Dash: {
      const uint16_t timeMs = 17000 - static_cast<uint16_t>(avg) * 65;
      const bool qualified = timeMs <= qualifyingTarget();
      const uint16_t score = qualified ? static_cast<uint16_t>(85 + max<int>(0, qualifyingTarget() - timeMs) / 38 + avg / 2) : 0;
      finishAttempt(qualified, score, timeMs, ResultUnit::Time);
      break;
    }
    case CueMode::LongRun:
      runQuality_ = avg;
      selectorAngle_ = 20.0f;
      selectorDir_ = 1;
      state_ = FieldState::AngleSelect;
      stateTimerMs_ = 0;
      break;
    case CueMode::HammerPower: {
      powerQuality_ = avg;
      const float forwardFactor = cosf(directionDeg_ * 0.0174533f);
      const uint16_t rawDistanceCm = 1200 + static_cast<uint16_t>(angleQuality_) * 22 +
                                     static_cast<uint16_t>(powerQuality_) * 25;
      const int16_t signedDistanceCm = static_cast<int16_t>(clampFloat(static_cast<float>(rawDistanceCm) * forwardFactor, -9500.0f, 9500.0f));
      throwSignedDistanceCm_ = signedDistanceCm;
      const uint16_t forwardDistanceCm = signedDistanceCm > 0 ? static_cast<uint16_t>(signedDistanceCm) : 0;
      const bool qualified = forwardDistanceCm >= qualifyingTarget();
      const uint16_t score = qualified ? static_cast<uint16_t>(80 + max<int>(0, forwardDistanceCm - qualifyingTarget()) / 55 + angleQuality_ / 3 + powerQuality_ / 2) : 0;
      finishAttempt(qualified, score, static_cast<uint16_t>(absInt(signedDistanceCm)), ResultUnit::Distance);
      break;
    }
    case CueMode::JavelinRun:
      runQuality_ = avg;
      selectorAngle_ = 15.0f;
      holding_ = false;
      state_ = FieldState::JavelinHoldAngle;
      stateTimerMs_ = 0;
      break;
    case CueMode::HighBonus: {
      const uint16_t heightCm = 88 + static_cast<uint16_t>(highAngleQuality_) * 7 / 10 +
                                static_cast<uint16_t>(avg) * 35 / 100 + static_cast<uint16_t>(round_) * 2;
      const bool qualified = heightCm >= qualifyingTarget();
      const uint16_t score = qualified ? static_cast<uint16_t>(75 + max<int>(0, heightCm - qualifyingTarget()) * 4 + highAngleQuality_ / 2 + avg / 3) : 0;
      finishAttempt(qualified, score, heightCm, ResultUnit::Height);
      break;
    }
  }
}

void MicriFieldGame::finishAttempt(bool qualified, uint16_t eventScore, uint16_t value, ResultUnit unit) {
  if (!qualified) {
    eventScore = 0;
    if (!pressureMode_ && attemptsRemaining_ > 0) {
      attemptsRemaining_--;
    }
  }
  qualified_ = qualified;
  resultScore_ = eventScore;
  resultValue_ = value;
  resultUnit_ = unit;
  bestAttemptScore_ = max(bestAttemptScore_, eventScore);
  bestAttemptValue_ = max(bestAttemptValue_, value);
  extraLifeEarned_ = false;
  state_ = (eventIndex_ == 3) ? FieldState::ThrowAnim : FieldState::Result;
  stateTimerMs_ = 0;
  throwAnimMs_ = 0;
  setFieldLed(false);

  if (qualified) {
    addScore(eventScore);
    if (eventScore > bestEventScores_[eventIndex_]) {
      bestEventScores_[eventIndex_] = eventScore;
      saveEventRecord(eventIndex_);
    }
  }
}

void MicriFieldGame::handleResultInput(const ButtonInput& b1, const ButtonInput& b2) {
  if (stateTimerMs_ < RESULT_LOCK_MS || !b1.click) {
    return;
  }
  if (qualified_) {
    advanceEvent();
    return;
  }
  if (!pressureMode_ && attemptsRemaining_ > 0) {
    startAttempt();
    return;
  }
  pressureMode_ = true;
  roundHadZero_ = true;
  advanceEvent();
}

void MicriFieldGame::advanceEvent() {
  eventIndex_++;
  if (eventIndex_ >= EVENT_COUNT) {
    if (roundHadZero_) {
      endApp();
      return;
    }
    eventIndex_ = 0;
    round_++;
    attemptsRemaining_ = STARTING_ATTEMPT_BANK;
    pressureMode_ = false;
    roundHadZero_ = false;
  }
  startEventIntro();
}

void MicriFieldGame::loseLifeOrRetry() {
  pressureMode_ = true;
  roundHadZero_ = true;
  advanceEvent();
}

void MicriFieldGame::addScore(uint16_t amount) {
  totalScore_ += amount;
  if (totalScore_ >= nextExtraLifeAt_) {
    nextExtraLifeAt_ += EXTRA_LIFE_POINTS;
  }
  if (totalScore_ > bestTotalScore_) {
    bestTotalScore_ = totalScore_;
    saveTotalRecord();
  }
}

void MicriFieldGame::setFieldLed(bool on) {
  if (!FIELD_LED_AVAILABLE) {
    return;
  }
  digitalWrite(FIELD_LED_PIN, FIELD_LED_ACTIVE_LOW ? !on : on);
}

void MicriFieldGame::updateCueLed() {
  const bool inPush = cueElapsedMs_ >= cueTargetMs_ && cueElapsedMs_ <= cueTargetMs_ + 120;
  setFieldLed(inPush);
}

uint8_t MicriFieldGame::timingQuality(int16_t errorMs) {
  const int penalty = errorMs < 0 ? absInt(errorMs) * 2 : absInt(errorMs);
  if (penalty >= 330) {
    return 0;
  }
  return static_cast<uint8_t>(100 - (penalty * 100 / 330));
}

uint8_t MicriFieldGame::angleQuality(float selected, float optimal, float tolerance) const {
  float diff = absFloat(selected - optimal);
  if (diff > 180.0f) {
    diff = 360.0f - diff;
  }
  if (diff >= tolerance) {
    return 0;
  }
  return static_cast<uint8_t>(100.0f - (diff * 100.0f / tolerance));
}

uint16_t MicriFieldGame::qualifyingTarget() const {
  const uint8_t r = round_ > 0 ? round_ - 1 : 0;
  switch (eventIndex_) {
    case 0:
      return max<uint16_t>(11200, 14500 - static_cast<uint16_t>(r) * 500);
    case 1:
      return max<uint16_t>(12600, 15800 - static_cast<uint16_t>(r) * 500);
    case 2:
      return 460 + static_cast<uint16_t>(r) * 40;
    case 3:
      return 3650 + static_cast<uint16_t>(r) * 260;
    case 4:
      return 3550 + static_cast<uint16_t>(r) * 280;
    case 5:
    default:
      return 128 + static_cast<uint16_t>(r) * 8;
  }
}

uint8_t MicriFieldGame::maxAttemptsForEvent() const {
  return pressureMode_ ? 1 : attemptsRemaining_;
}

bool MicriFieldGame::isFieldEvent() const {
  return eventIndex_ >= 2;
}

const char* MicriFieldGame::eventName(uint8_t index) const {
  return EVENT_NAMES[index % EVENT_COUNT];
}

void MicriFieldGame::loadRecords() {
  if (bestLoaded_) {
    return;
  }
  migrateLegacyFieldPrefs();
  fieldPrefs.begin("micrifld", true);
  bestTotalScore_ = fieldPrefs.getUInt("total", 0);
  bestInitials_ = fieldPrefs.getUShort("init", PlayerProfile::defaultInitials());
  for (uint8_t i = 0; i < EVENT_COUNT; i++) {
    char key[5];
    snprintf(key, sizeof(key), "ev%u", i);
    bestEventScores_[i] = fieldPrefs.getUShort(key, 0);
  }
  fieldPrefs.end();
  bestLoaded_ = true;
}

void MicriFieldGame::saveTotalRecord() {
  bestInitials_ = PlayerProfile::loadInitials();
  fieldPrefs.begin("micrifld", false);
  fieldPrefs.putUInt("total", bestTotalScore_);
  fieldPrefs.putUShort("init", bestInitials_);
  fieldPrefs.end();
}

void MicriFieldGame::saveEventRecord(uint8_t eventIndex) {
  bestInitials_ = PlayerProfile::loadInitials();
  fieldPrefs.begin("micrifld", false);
  char key[5];
  snprintf(key, sizeof(key), "ev%u", eventIndex);
  fieldPrefs.putUShort(key, bestEventScores_[eventIndex]);
  fieldPrefs.putUShort("init", bestInitials_);
  fieldPrefs.end();
}

void MicriFieldGame::drawRunning(TFT_eSPI& tft) {
  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    switch (state_) {
      case FieldState::EventIntro:
        drawEventIntro(canvas);
        break;
      case FieldState::CueSequence:
        drawCueSequence(canvas);
        break;
      case FieldState::HurdleCue:
      case FieldState::HurdleHold:
        drawHurdles(canvas);
        break;
      case FieldState::DirectionSelect:
        drawDirectionSelect(canvas);
        break;
      case FieldState::AngleSelect:
        drawAngleSelect(canvas);
        break;
      case FieldState::JavelinHoldAngle:
        drawJavelinHoldAngle(canvas);
        break;
      case FieldState::HighJumpHold:
        drawHighJumpHold(canvas);
        break;
      case FieldState::ThrowAnim:
        drawThrowAnim(canvas);
        break;
      case FieldState::Result:
        drawResult(canvas);
        break;
    }
  });
}

void MicriFieldGame::drawStart(TFT_eSPI& tft) {
  TDisplayUi::clear(tft);
  loadRecords();
  if (showStartPromptPage()) {
    TDisplayUi::header(tft, appTitle(), TFT_ORANGE);
    TDisplayUi::centered(tft, "Press", 50, 3, TFT_WHITE);
    TDisplayUi::centered(tft, "to Start", 78, 2, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "B1 start");
    return;
  }
  if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    const uint8_t eventRecord = (millis() / 650) % EVENT_COUNT;

    TDisplayUi::header(tft, "Top Total", TFT_YELLOW);
    TDisplayUi::largeValue(tft, bestTotalScore_ == 0 ? String("--") : String(bestTotalScore_), 47, TFT_YELLOW);
    TDisplayUi::centered(tft, bestTotalScore_ == 0 ? String("") : String(initials), 86, 2, TFT_LIGHTGREY);
    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(String(eventName(eventRecord)) + " best " + String(bestEventScores_[eventRecord]), 10, 108);
    TDisplayUi::footer(tft, "Event records rotate");
    return;
  }
  drawTrackSplash(tft);
}

void MicriFieldGame::drawEnd(TFT_eSPI& tft) {
  TDisplayUi::clear(tft);
  setFieldLed(false);
  TDisplayUi::header(tft, "Game Over", TFT_RED);
  TDisplayUi::labelValue(tft, 48, "Score", String(totalScore_), TFT_ORANGE);
  TDisplayUi::labelValue(tft, 78, "Best", String(bestTotalScore_), TFT_YELLOW);
  TDisplayUi::footer(tft, "B1 retry  Hold menu");
}

template <typename Canvas>
void MicriFieldGame::drawScoreHeader(Canvas& tft) {
  const String stat = "R" + String(round_) + " A" + String(attemptsRemaining_) + " S" + String(totalScore_);
  TDisplayUi::header(tft, eventName(eventIndex_), TFT_ORANGE, stat.c_str());
}

template <typename Canvas>
void MicriFieldGame::drawEventIntro(Canvas& tft) {
  drawScoreHeader(tft);

  TDisplayUi::centered(tft, eventName(eventIndex_), 42, 3, TFT_WHITE);
  TDisplayUi::labelValue(tft, 84, "Qual", formatEventValue(eventIndex_, qualifyingTarget()), TFT_YELLOW);
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  if (pressureMode_) {
    tft.drawString("Pressure mode: 1 try", 10, 108);
  } else {
    tft.drawString("Attempts left " + String(attemptsRemaining_), 10, 108);
  }
  TDisplayUi::footer(tft, "B1 start event");
}

template <typename Canvas>
void MicriFieldGame::drawCueSequence(Canvas& tft) {
  drawScoreHeader(tft);
  const bool push = cueElapsedMs_ >= cueTargetMs_;

  TDisplayUi::largeValue(tft, push ? String("PUSH") : String("WAIT"), 45, push ? TFT_GREEN : TFT_YELLOW);
  if (messageTimerMs_ > 0) {
    TDisplayUi::centered(tft, "Early x2 penalty", 88, 2, TFT_RED);
  } else {
    TDisplayUi::centered(tft, "Quality " + String(lastQuality_), 89, 2, TFT_LIGHTGREY);
  }
  TDisplayUi::bar(tft, 42, 108, 156, 8, cueTotal_ == 0 ? 0.0f : static_cast<float>(cueDone_) / static_cast<float>(cueTotal_), TFT_CYAN);
  if (cueMode_ == CueMode::LongRun || cueMode_ == CueMode::JavelinRun || cueMode_ == CueMode::Dash) {
    drawStick(tft, 22 + (cueDone_ * 16), 112, false);
  }
}

template <typename Canvas>
void MicriFieldGame::drawHurdles(Canvas& tft) {
  drawScoreHeader(tft);
  const bool airborne = hurdleRunnerY_ > 0.2f;
  const int hurdleX = static_cast<int>(hurdleX_);
  const int ground = 112;
  tft.drawFastHLine(0, ground, width, TFT_DARKGREEN);
  tft.drawFastHLine(0, ground + 1, width, TFT_DARKGREEN);
  drawStick(tft, 43, ground - static_cast<int>(hurdleRunnerY_ * 1.75f), airborne);
  if (hurdleX >= 2 && hurdleX <= static_cast<int>(width - 14)) {
    drawHurdle(tft, hurdleX, ground, hurdleHit_);
  }
  if (!hurdleJumped_) {
    TDisplayUi::centered(tft, "TAP JUMP", 42, 2, TFT_YELLOW);
  }

  const String hurdleFooter = "H" + String(hurdleIndex_ + 1) + "/8  Cleared " + String(hurdlesCleared_);
  TDisplayUi::footer(tft, hurdleFooter.c_str());
}

template <typename Canvas>
void MicriFieldGame::drawDirectionSelect(Canvas& tft) {
  drawScoreHeader(tft);
  const int cx = 84;
  const int cy = 77;
  tft.drawCircle(cx, cy, 5, TFT_WHITE);
  tft.drawCircle(cx, cy, 42, TFT_DARKGREY);
  const float rad = directionDeg_ * 0.0174533f;
  const int bx = cx + static_cast<int>(cosf(rad) * 42);
  const int by = cy + static_cast<int>(sinf(rad) * 42);
  tft.drawLine(cx, cy, bx, by, TFT_YELLOW);
  tft.fillCircle(bx, by, 4, TFT_YELLOW);
  tft.drawLine(cx, cy, cx + 50, cy, TFT_GREEN);

  TDisplayUi::centered(tft, "Aim ahead", 42, 2, TFT_LIGHTGREY);
  TDisplayUi::footer(tft, "B1 choose throw direction");
}

template <typename Canvas>
void MicriFieldGame::drawAngleSelect(Canvas& tft) {
  drawScoreHeader(tft);
  drawAngleFan(tft, 38, 108, selectorAngle_);

  TDisplayUi::centered(tft, "ANGLE", 42, 2, TFT_YELLOW);
  TDisplayUi::largeValue(tft, String(static_cast<int>(selectorAngle_)) + "deg", 62, TFT_WHITE);
  TDisplayUi::footer(tft, "B1 select angle");
}

template <typename Canvas>
void MicriFieldGame::drawJavelinHoldAngle(Canvas& tft) {
  drawScoreHeader(tft);
  drawAngleFan(tft, 38, 108, selectorAngle_);
  tft.drawLine(24, 110, 116, 78, TFT_LIGHTGREY);

  TDisplayUi::centered(tft, holding_ ? "Release" : "Hold", 42, 2, holding_ ? TFT_GREEN : TFT_YELLOW);
  TDisplayUi::largeValue(tft, String(static_cast<int>(selectorAngle_)) + "deg", 62, TFT_WHITE);
  TDisplayUi::footer(tft, "Hold for javelin angle");
}

template <typename Canvas>
void MicriFieldGame::drawHighJumpHold(Canvas& tft) {
  drawScoreHeader(tft);
  const int ground = 112;
  tft.drawFastHLine(0, ground, width, TFT_DARKGREEN);
  tft.drawLine(154, 58, 154, ground, TFT_LIGHTGREY);
  tft.drawLine(196, 58, 196, ground, TFT_LIGHTGREY);
  tft.drawLine(154, 58, 196, 58, TFT_YELLOW);
  const int jumpY = holding_ ? ground - min<int>(46, highHoldMs_ / 16) : ground;
  drawStick(tft, 72, jumpY, holding_);
  TDisplayUi::centered(tft, holding_ ? "Release" : "Hold", 42, 2, holding_ ? TFT_GREEN : TFT_YELLOW);
  TDisplayUi::footer(tft, "Hold to set jump angle");
}

template <typename Canvas>
void MicriFieldGame::drawThrowAnim(Canvas& tft) {
  TDisplayUi::header(tft, "Hammer", TFT_ORANGE);

  const int originX = 42;
  const int groundY = 110;
  tft.drawFastHLine(0, groundY, width, TFT_DARKGREEN);
  drawStick(tft, originX - 5, groundY, false);

  const float progress = min(1.0f, static_cast<float>(throwAnimMs_) / static_cast<float>(THROW_ANIM_MS));
  const float distanceM = static_cast<float>(throwSignedDistanceCm_) / 100.0f;
  const int travelPx = constrain(static_cast<int>(distanceM * 3.0f), -42, 160);
  const int ballX = originX + static_cast<int>(static_cast<float>(travelPx) * progress);
  const int arc = static_cast<int>(sinf(progress * 3.14159f) * 45.0f);
  const int ballY = groundY - 2 - arc;
  tft.fillCircle(ballX, ballY, 5, TFT_LIGHTGREY);
  tft.drawLine(originX, groundY - 16, ballX, ballY, TFT_LIGHTGREY);

  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(150, 52);
  if (throwSignedDistanceCm_ < 0) {
    tft.print("-");
  }
  tft.print(absInt(throwSignedDistanceCm_) / 100);
  tft.print(".");
  tft.print((absInt(throwSignedDistanceCm_) / 10) % 10);
  tft.print("m");
  TDisplayUi::footer(tft, "Throw distance");
}

template <typename Canvas>
void MicriFieldGame::drawResult(Canvas& tft) {
  TDisplayUi::header(tft, qualified_ ? "Qualified" : "No Qual", qualified_ ? TFT_GREEN : TFT_RED);

  String value;
  if (resultUnit_ == ResultUnit::Time) {
    value = formatEventValue(0, resultValue_);
    if (eventIndex_ == 1) {
      value += " C";
      value += String(hurdlesCleared_);
    }
  } else {
    if (eventIndex_ == 3 && throwSignedDistanceCm_ < 0) {
      value += "-";
    }
    const uint16_t displayValue = eventIndex_ == 3 ? static_cast<uint16_t>(absInt(throwSignedDistanceCm_)) : resultValue_;
    value += formatEventValue(eventIndex_, displayValue);
  }
  TDisplayUi::labelValue(tft, 45, resultUnit_ == ResultUnit::Time ? "Time" : (resultUnit_ == ResultUnit::Height ? "Height" : "Dist"), value, TFT_YELLOW);
  TDisplayUi::labelValue(tft, 75, "Points", String(resultScore_), TFT_GREEN);
  TDisplayUi::labelValue(tft, 101, "Total", String(totalScore_), TFT_ORANGE);
  if (qualified_) {
    TDisplayUi::footer(tft, "B1 next event");
  } else if (!pressureMode_ && attemptsRemaining_ > 0) {
    TDisplayUi::footer(tft, ("B1 retry  Attempts " + String(attemptsRemaining_)).c_str());
  } else {
    TDisplayUi::footer(tft, "B1 next  0 points");
  }
}

template <typename Canvas>
void MicriFieldGame::drawStick(Canvas& tft, int x, int groundY, bool airborne) {
  const int y = groundY - 24;
  tft.drawCircle(x, y - 8, 5, TFT_WHITE);
  tft.drawLine(x, y - 3, x, y + 14, TFT_WHITE);
  tft.drawLine(x, y + 5, x + 12, y + (airborne ? 0 : 9), TFT_WHITE);
  tft.drawLine(x, y + 5, x - 12, y + (airborne ? 2 : 11), TFT_WHITE);
  tft.drawLine(x, y + 14, x + 12, groundY, TFT_WHITE);
  tft.drawLine(x, y + 14, x - 9, groundY - (airborne ? 12 : 0), TFT_WHITE);
}

template <typename Canvas>
void MicriFieldGame::drawHurdle(Canvas& tft, int x, int groundY, bool fallen) {
  if (fallen) {
    tft.drawLine(x, groundY - 3, x + 28, groundY - 13, TFT_RED);
    tft.drawLine(x + 4, groundY, x + 11, groundY - 9, TFT_LIGHTGREY);
    tft.drawLine(x + 25, groundY, x + 19, groundY - 11, TFT_LIGHTGREY);
    return;
  }
  tft.drawLine(x, groundY, x + 5, groundY - 28, TFT_WHITE);
  tft.drawLine(x + 34, groundY, x + 29, groundY - 28, TFT_WHITE);
  tft.drawLine(x + 5, groundY - 28, x + 29, groundY - 28, TFT_YELLOW);
}

template <typename Canvas>
void MicriFieldGame::drawAngleFan(Canvas& tft, int ox, int oy, float angleDeg) {
  tft.drawLine(ox, oy, ox + 138, oy, TFT_DARKGREY);
  tft.drawLine(ox, oy, ox + 98, oy - 72, TFT_DARKGREY);
  tft.drawArc(ox, oy, 58, 56, 315, 360, TFT_DARKGREY, TFT_BLACK);
  const float rad = -angleDeg * 0.0174533f;
  const int x2 = ox + static_cast<int>(cosf(rad) * 118);
  const int y2 = oy + static_cast<int>(sinf(rad) * 118);
  tft.drawLine(ox, oy, x2, y2, TFT_YELLOW);
  tft.fillCircle(x2, y2, 4, TFT_YELLOW);
}

template <typename Canvas>
void MicriFieldGame::drawTrackSplash(Canvas& tft) {
  TDisplayUi::header(tft, "Micri Field", TFT_ORANGE);
  TDisplayUi::centered(tft, "Track & Field", 36, 2, TFT_LIGHTGREY);
  tft.drawFastHLine(0, 110, width, TFT_DARKGREEN);
  tft.drawFastHLine(0, 118, width, TFT_DARKGREEN);
  drawStick(tft, 55, 110, false);
  drawHurdle(tft, 116, 110);
  tft.drawLine(174, 82, 219, 50, TFT_YELLOW);
  tft.fillCircle(224, 47, 3, TFT_LIGHTGREY);
  TDisplayUi::footer(tft, "Six arcade events");
}
