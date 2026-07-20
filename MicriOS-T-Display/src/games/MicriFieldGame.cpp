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
constexpr uint8_t HURDLE_COUNT = 10;
constexpr uint16_t RESULT_ANIM_MS = 1700;
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
  resultAnimMs_ = 0;
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
      hurdleX_ = static_cast<float>(width) * 0.82f;
      hurdleSpeed_ = 76.0f + static_cast<float>(round_) * 4.0f;
      hurdleScored_ = false;
      hurdleHit_ = false;
      hurdleJumped_ = false;
      hurdleJumpAgeMs_ = 0;
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

  if (state_ == FieldState::ResultAnim) {
    resultAnimMs_ += cappedDelta;
    setFieldLed(false);
    if (resultAnimMs_ >= RESULT_ANIM_MS) {
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
      hurdleJumpAgeMs_ = 0;
    }

    hurdleRunnerY_ += hurdleVy_ * dt;
    hurdleVy_ -= 88.0f * dt;
    if (hurdleRunnerY_ < 0.0f) {
      hurdleRunnerY_ = 0.0f;
      hurdleVy_ = 0.0f;
      hurdleJumpAgeMs_ = 0;
    } else if (hurdleRunnerY_ > 0.1f) {
      hurdleJumpAgeMs_ = static_cast<uint16_t>(min<uint32_t>(2000, hurdleJumpAgeMs_ + cappedDelta));
    }

    hurdleX_ -= hurdleSpeed_ * dt * (hurdleRunnerY_ > 0.1f ? 0.82f : 1.0f);
    const float runnerX = 28.0f;
    if (!hurdleScored_ && hurdleX_ <= runnerX + 2.0f) {
      const bool cleared = hurdleRunnerY_ >= 7.0f;
      if (cleared) {
        hurdlesCleared_++;
        const int heightError = absInt(static_cast<int>(hurdleRunnerY_ - 11.0f));
        const int earlyPenalty = hurdleJumpAgeMs_ > 520
                                     ? min<int>(45, (hurdleJumpAgeMs_ - 520) / 8)
                                     : 0;
        runQuality_ += static_cast<uint8_t>(max<int>(20, 100 - heightError * 5 - earlyPenalty));
        hurdleHit_ = false;
      } else {
        runQuality_ += 8;
        hurdleHit_ = true;
      }
      hurdleIndex_++;
      hurdleScored_ = true;
    }

    if (hurdleX_ < -12.0f) {
      if (hurdleIndex_ >= HURDLE_COUNT) {
        const uint8_t avg = runQuality_ / HURDLE_COUNT;
        const uint16_t timeMs = 19000 - static_cast<uint16_t>(hurdlesCleared_) * 430 -
                                static_cast<uint16_t>(avg) * 12 - static_cast<uint16_t>(round_) * 80;
        const bool qualified = hurdlesCleared_ >= 8 && timeMs <= qualifyingTarget();
        const uint16_t score = qualified ? static_cast<uint16_t>(80 + hurdlesCleared_ * 10 + max<int>(0, qualifyingTarget() - timeMs) / 35 + avg / 2) : avg / 3;
        finishAttempt(qualified, score, timeMs, ResultUnit::Time);
      } else {
        hurdleX_ = static_cast<float>(width) * 0.72f +
                   static_cast<float>(random(0, max<int>(2, width / 12)));
        hurdleSpeed_ += 0.8f;
        hurdleScored_ = false;
        hurdleHit_ = false;
        hurdleJumpAgeMs_ = 0;
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
  state_ = eventIndex_ == 1 ? FieldState::Result : FieldState::ResultAnim;
  stateTimerMs_ = 0;
  resultAnimMs_ = 0;
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
      case FieldState::ResultAnim:
        drawResultAnim(canvas);
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
  const String title = String(eventName(eventIndex_)) + " R" + String(round_);
  const String target = "Q " + formatEventValue(eventIndex_, qualifyingTarget());
  TDisplayUi::header(tft, title.c_str(), TFT_ORANGE, target.c_str());
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

  const uint8_t shownHurdle = min<uint8_t>(HURDLE_COUNT,
      hurdleIndex_ + (hurdleScored_ ? 0 : 1));
  const String hurdleFooter = "H" + String(shownHurdle) + "/" +
                              String(HURDLE_COUNT) + "  Cleared " +
                              String(hurdlesCleared_);
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
void MicriFieldGame::drawResultAnim(Canvas& tft) {
  const float progress = min(1.0f, static_cast<float>(resultAnimMs_) /
                                       static_cast<float>(RESULT_ANIM_MS));
  const uint8_t gait = static_cast<uint8_t>((resultAnimMs_ / 90) & 1U);
  const int groundY = 110;
  const String target = "Q " + formatEventValue(eventIndex_, qualifyingTarget());
  TDisplayUi::header(tft, eventName(eventIndex_), TFT_ORANGE, target.c_str());

  if (eventIndex_ == 0) {
    const int finishX = 222;
    const int laneGround[3] = {57, 84, 111};
    for (uint8_t lane = 0; lane < 3; ++lane) {
      tft.drawFastHLine(0, laneGround[lane] + 1, width, TFT_DARKGREY);
    }
    for (int y = 29; y < groundY; y += 8) {
      tft.fillRect(finishX, y, 3, 4, ((y / 4) & 1) ? TFT_WHITE : TFT_DARKGREY);
    }
    const float playerPace = clampFloat(
        static_cast<float>(qualifyingTarget()) / max<uint16_t>(1, resultValue_),
        0.72f, 1.10f);
    const float pace[3] = {0.92f, playerPace, 1.01f};
    const uint16_t colors[3] = {TFT_CYAN, TFT_GREEN, TFT_MAGENTA};
    for (uint8_t lane = 0; lane < 3; ++lane) {
      const int runnerX = 14 + static_cast<int>(208.0f * min(1.0f, progress * pace[lane]));
      drawStick(tft, runnerX, laneGround[lane], false, colors[lane], gait + lane);
    }
    TDisplayUi::footer(tft, ("You " + formatEventValue(0, resultValue_)).c_str());
    return;
  }

  tft.drawFastHLine(0, groundY, width, TFT_DARKGREEN);

  if (eventIndex_ == 2) {
    const int takeoffX = 86;
    const float ratio = clampFloat(static_cast<float>(resultValue_) /
                                       static_cast<float>(qualifyingTarget()),
                                   0.55f, 1.35f);
    const int landingX = min<int>(229, takeoffX + static_cast<int>(96.0f * ratio));
    tft.fillRect(takeoffX, groundY - 4, 4, 6, TFT_WHITE);
    tft.fillRect(takeoffX + 6, groundY + 1, width - takeoffX - 12, 5, TFT_YELLOW);
    const float runP = min(1.0f, progress / 0.28f);
    const float flightP = progress <= 0.28f ? 0.0f : (progress - 0.28f) / 0.72f;
    const int athleteX = progress <= 0.28f
                             ? 20 + static_cast<int>((takeoffX - 20) * runP)
                             : takeoffX + static_cast<int>((landingX - takeoffX) * flightP);
    const int lift = progress <= 0.28f ? 0 : static_cast<int>(sinf(flightP * PI) * 38.0f);
    drawStick(tft, athleteX, groundY - lift, lift > 2, TFT_GREEN, gait);
    tft.drawFastVLine(landingX, groundY - 8, 8, TFT_CYAN);
    TDisplayUi::footer(tft, ("Dist " + formatEventValue(2, resultValue_)).c_str());
    return;
  }

  if (eventIndex_ == 3) {
    const int originX = 42;
    drawStick(tft, originX - 5, groundY, false, TFT_ORANGE, gait);
    const float distanceM = static_cast<float>(throwSignedDistanceCm_) / 100.0f;
    const int travelPx = constrain(static_cast<int>(distanceM * 3.0f), -42, 190);
    const int ballX = originX + static_cast<int>(static_cast<float>(travelPx) * progress);
    const int ballY = groundY - 2 - static_cast<int>(sinf(progress * PI) * 45.0f);
    const float dx = static_cast<float>(ballX - originX);
    const float dy = static_cast<float>(ballY - (groundY - 16));
    const float length = sqrtf(dx * dx + dy * dy);
    const float tether = min(18.0f, length);
    const int tailX = length > 0.1f ? ballX - static_cast<int>(dx / length * tether) : originX;
    const int tailY = length > 0.1f ? ballY - static_cast<int>(dy / length * tether) : groundY - 16;
    tft.drawLine(tailX, tailY, ballX, ballY, TFT_LIGHTGREY);
    tft.fillCircle(ballX, ballY, 4, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, ("Dist " +
        String(throwSignedDistanceCm_ < 0 ? "-" : "") +
        formatEventValue(3, static_cast<uint16_t>(absInt(throwSignedDistanceCm_)))).c_str());
    return;
  }

  if (eventIndex_ == 4) {
    const int originX = 42;
    drawStick(tft, originX - 6, groundY, false, TFT_CYAN, gait);
    const float ratio = clampFloat(static_cast<float>(resultValue_) /
                                       static_cast<float>(qualifyingTarget()),
                                   0.55f, 1.35f);
    const int travel = static_cast<int>(180.0f * min(1.0f, ratio));
    const int spearX = originX + static_cast<int>(travel * progress);
    const int spearY = groundY - 18 - static_cast<int>(sinf(progress * PI) * 44.0f);
    tft.drawLine(spearX - 22, spearY + 5, spearX + 6, spearY - 2, TFT_YELLOW);
    tft.fillTriangle(spearX + 6, spearY - 2, spearX, spearY - 4,
                     spearX + 1, spearY + 1, TFT_WHITE);
    TDisplayUi::footer(tft, ("Dist " + formatEventValue(4, resultValue_)).c_str());
    return;
  }

  const int leftPost = 125;
  const int rightPost = 171;
  const int barY = 58;
  const bool barFallen = !qualified_ && progress > 0.68f;
  tft.drawFastVLine(leftPost, barY, groundY - barY, TFT_LIGHTGREY);
  tft.drawFastVLine(rightPost, barY, groundY - barY, TFT_LIGHTGREY);
  if (barFallen) {
    tft.drawLine(leftPost, barY + 3, rightPost, groundY - 6, TFT_RED);
  } else {
    tft.drawFastHLine(leftPost, barY, rightPost - leftPost, TFT_YELLOW);
  }
  const int athleteX = 32 + static_cast<int>(160.0f * progress);
  const int peak = qualified_ ? 62 : 42;
  const int lift = static_cast<int>(sinf(progress * PI) * peak);
  drawStick(tft, athleteX, groundY - lift, lift > 2,
            qualified_ ? TFT_GREEN : TFT_RED, gait);
  TDisplayUi::footer(tft, (String(qualified_ ? "Cleared " : "Missed ") +
                           formatEventValue(5, resultValue_)).c_str());
}

template <typename Canvas>
void MicriFieldGame::drawResult(Canvas& tft) {
  const String target = "Need " + formatEventValue(eventIndex_, qualifyingTarget());
  TDisplayUi::header(tft, qualified_ ? "Qualified" : "No Qual",
                     qualified_ ? TFT_GREEN : TFT_RED, target.c_str());

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
void MicriFieldGame::drawStick(Canvas& tft, int x, int groundY, bool airborne,
                               uint16_t color, uint8_t gait) {
  const int y = groundY - 20;
  const int stride = (gait & 1U) ? 8 : -8;
  tft.fillCircle(x, y - 6, 4, TFT_ORANGE);
  tft.drawCircle(x, y - 6, 4, TFT_WHITE);
  tft.fillRoundRect(x - 2, y - 1, 5, 12, 2, color);
  tft.fillRect(x - 3, y + 9, 7, 4, TFT_BLUE);
  const int armLift = airborne ? -3 : ((gait & 1U) ? 3 : -1);
  tft.drawLine(x - 1, y + 2, x + 9, y + armLift, color);
  tft.drawLine(x + 1, y + 3, x - 8, y + 7 - armLift, color);
  tft.drawLine(x - 1, y + 12, x + stride, groundY - (airborne ? 7 : 0), TFT_WHITE);
  tft.drawLine(x + 1, y + 12, x - stride, groundY - (airborne ? 10 : 0), TFT_WHITE);
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
