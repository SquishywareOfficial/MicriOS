#include "MicriFieldGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
constexpr uint8_t FIELD_LED_PIN = 8;
constexpr bool FIELD_LED_ACTIVE_LOW = true;
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

MicriFieldGame::MicriFieldGame(uint32_t width, uint32_t height, uint32_t left)
    : App("Micri Field", width, height), left_(left) {}

bool MicriFieldGame::hasCustomOverlay() const {
  return true;
}

void MicriFieldGame::onAppReset() {
  pinMode(FIELD_LED_PIN, OUTPUT);
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
      hurdleSpeed_ = 23.0f + static_cast<float>(round_) * 1.4f;
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

void MicriFieldGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  const uint16_t cappedDelta = static_cast<uint16_t>(min<uint32_t>(deltaMs, 180));
  stateTimerMs_ += cappedDelta;
  if (messageTimerMs_ > cappedDelta) {
    messageTimerMs_ -= cappedDelta;
  } else {
    messageTimerMs_ = 0;
  }

  if (state_ == FieldState::EventIntro) {
    setFieldLed(false);
    if (stateTimerMs_ >= 450 && input.click) {
      startAttempt();
    }
    return;
  }

  if (state_ == FieldState::Result) {
    setFieldLed(false);
    handleResultInput(input);
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
    if (input.pressed) {
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
    if ((input.click || input.pressed) && hurdleRunnerY_ <= 0.1f) {
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
    if (input.pressed) {
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
    if (!input.pressed) {
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
    if (!holding_ && input.pressed) {
      holding_ = true;
      selectorAngle_ = 15.0f;
    } else if (holding_ && input.down) {
      selectorAngle_ += static_cast<float>(cappedDelta) * (0.072f + static_cast<float>(round_) * 0.004f);
      if (selectorAngle_ > 70.0f) {
        selectorAngle_ = 70.0f;
      }
    }

    if (holding_ && (input.released || selectorAngle_ >= 70.0f)) {
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
    if (!holding_ && input.pressed) {
      holding_ = true;
      highHoldMs_ = 0;
    } else if (holding_ && input.down && highHoldMs_ < 760) {
      highHoldMs_ += cappedDelta;
    }

    if (holding_ && (input.released || highHoldMs_ >= 760)) {
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

void MicriFieldGame::handleResultInput(const ButtonInput& input) {
  if (stateTimerMs_ < RESULT_LOCK_MS || !input.click) {
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

void MicriFieldGame::drawRunning(U8G2& u8g2) {
  switch (state_) {
    case FieldState::EventIntro:
      drawEventIntro(u8g2);
      break;
    case FieldState::CueSequence:
      drawCueSequence(u8g2);
      break;
    case FieldState::HurdleCue:
    case FieldState::HurdleHold:
      drawHurdles(u8g2);
      break;
    case FieldState::DirectionSelect:
      drawDirectionSelect(u8g2);
      break;
    case FieldState::AngleSelect:
      drawAngleSelect(u8g2);
      break;
    case FieldState::JavelinHoldAngle:
      drawJavelinHoldAngle(u8g2);
      break;
    case FieldState::HighJumpHold:
      drawHighJumpHold(u8g2);
      break;
    case FieldState::ThrowAnim:
      drawThrowAnim(u8g2);
      break;
    case FieldState::Result:
      drawResult(u8g2);
      break;
  }
}

void MicriFieldGame::drawStart(U8G2& u8g2) {
  loadRecords();
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
    const uint8_t eventRecord = (millis() / 650) % EVENT_COUNT;
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 9, "Top Total");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(3, 19);
    if (bestTotalScore_ == 0) {
      u8g2.print("--");
    } else {
      u8g2.print(initials);
      u8g2.print(" ");
      u8g2.print(bestTotalScore_);
    }
    u8g2.setCursor(3, 29);
    u8g2.print(eventName(eventRecord));
    u8g2.setCursor(3, 38);
    u8g2.print("Best ");
    u8g2.print(bestEventScores_[eventRecord]);
    return;
  }
  drawTrackSplash(u8g2);
}

void MicriFieldGame::drawEnd(U8G2& u8g2) {
  setFieldLed(false);
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, "Game Over");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(3, 20);
  u8g2.print("Score ");
  u8g2.print(totalScore_);
  u8g2.setCursor(3, 29);
  u8g2.print("Best ");
  u8g2.print(bestTotalScore_);
  u8g2.drawStr(3, 38, "Tap retry Hold menu");
}

void MicriFieldGame::drawScoreHeader(U8G2& u8g2) {
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 2, 6);
  u8g2.print("R");
  u8g2.print(round_);
  u8g2.print(" A");
  u8g2.print(attemptsRemaining_);
  u8g2.print(" S");
  u8g2.print(totalScore_);
}

void MicriFieldGame::drawEventIntro(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  drawScoreHeader(u8g2);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(left_ + 3, 17, eventName(eventIndex_));
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 3, 27);
  u8g2.print("Qual ");
  if (eventIndex_ <= 1) {
    u8g2.print(qualifyingTarget() / 1000);
    u8g2.print(".");
    u8g2.print((qualifyingTarget() / 100) % 10);
    u8g2.print("s");
  } else if (eventIndex_ == 5) {
    u8g2.print(qualifyingTarget() / 100);
    u8g2.print(".");
    u8g2.print((qualifyingTarget() / 10) % 10);
    u8g2.print("m");
  } else {
    u8g2.print(qualifyingTarget() / 100);
    u8g2.print(".");
    u8g2.print((qualifyingTarget() / 10) % 10);
    u8g2.print("m");
  }
  u8g2.setCursor(left_ + 3, 38);
  if (pressureMode_) {
    u8g2.print("Pressure 1 try");
  } else {
    u8g2.print("Attempts left ");
    u8g2.print(attemptsRemaining_);
  }
}

void MicriFieldGame::drawCueSequence(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  drawScoreHeader(u8g2);
  const bool push = cueElapsedMs_ >= cueTargetMs_;
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(left_ + 22, 18, push ? "PUSH" : "WAIT");
  if (messageTimerMs_ > 0) {
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(left_ + 9, 27, "Early x2");
  } else {
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(left_ + 4, 27);
    u8g2.print("Q ");
    u8g2.print(lastQuality_);
  }
  const int barW = map(cueDone_, 0, cueTotal_, 0, 60);
  u8g2.drawFrame(left_ + 5, 32, 61, 4);
  if (barW > 0) {
    u8g2.drawBox(left_ + 6, 33, barW, 2);
  }
  if (cueMode_ == CueMode::LongRun || cueMode_ == CueMode::JavelinRun || cueMode_ == CueMode::Dash) {
    drawStick(u8g2, left_ + 9 + (cueDone_ * 4), 31, false);
  }
}

void MicriFieldGame::drawHurdles(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  drawScoreHeader(u8g2);
  const bool airborne = hurdleRunnerY_ > 0.2f;
  const int hurdleX = left_ + static_cast<int>(hurdleX_);
  u8g2.drawLine(left_ + 2, 33, left_ + width - 3, 33);
  drawStick(u8g2, left_ + 18, 33 - static_cast<int>(hurdleRunnerY_), airborne);
  if (hurdleX >= static_cast<int>(left_) + 2 && hurdleX <= static_cast<int>(left_ + width - 14)) {
    drawHurdle(u8g2, hurdleX, 33, hurdleHit_);
  }
  if (!hurdleJumped_) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(left_ + 17, 17, "TAP JUMP");
  }
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 3, 39);
  u8g2.print("H");
  u8g2.print(hurdleIndex_ + 1);
  u8g2.print("/8 C");
  u8g2.print(hurdlesCleared_);
}

void MicriFieldGame::drawDirectionSelect(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  drawScoreHeader(u8g2);
  const int cx = left_ + 35;
  const int cy = 24;
  u8g2.drawCircle(cx, cy, 3);
  u8g2.drawCircle(cx, cy, 15);
  const float rad = directionDeg_ * 0.0174533f;
  const int bx = cx + static_cast<int>(cosf(rad) * 15);
  const int by = cy + static_cast<int>(sinf(rad) * 15);
  u8g2.drawLine(cx, cy, bx, by);
  u8g2.drawDisc(bx, by, 2);
  u8g2.drawLine(cx, cy, cx + 17, cy);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(left_ + 3, 14, "Aim ahead");
}

void MicriFieldGame::drawAngleSelect(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  drawScoreHeader(u8g2);
  drawAngleFan(u8g2, left_ + 12, 32, selectorAngle_);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(left_ + 31, 17, "ANGLE");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 34, 27);
  u8g2.print(static_cast<int>(selectorAngle_));
  u8g2.print(" deg");
}

void MicriFieldGame::drawJavelinHoldAngle(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  drawScoreHeader(u8g2);
  drawAngleFan(u8g2, left_ + 12, 32, selectorAngle_);
  u8g2.drawLine(left_ + 8, 33, left_ + 32, 24);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(left_ + 32, 15, holding_ ? "Release" : "Hold");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 35, 27);
  u8g2.print(static_cast<int>(selectorAngle_));
  u8g2.print(" deg");
}

void MicriFieldGame::drawHighJumpHold(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  drawScoreHeader(u8g2);
  u8g2.drawLine(left_ + 6, 33, left_ + 64, 33);
  u8g2.drawLine(left_ + 48, 16, left_ + 48, 33);
  u8g2.drawLine(left_ + 62, 16, left_ + 62, 33);
  u8g2.drawLine(left_ + 48, 16, left_ + 62, 16);
  const int jumpY = holding_ ? 31 - min<int>(14, highHoldMs_ / 45) : 33;
  drawStick(u8g2, left_ + 27, jumpY, holding_);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(left_ + 5, 15, holding_ ? "Release" : "Hold");
}

void MicriFieldGame::drawThrowAnim(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(left_ + 3, 7, "Hammer");

  const int originX = left_ + 18;
  const int groundY = 32;
  u8g2.drawLine(left_ + 3, groundY, left_ + width - 4, groundY);
  drawStick(u8g2, originX - 5, groundY, false);

  const float progress = min(1.0f, static_cast<float>(throwAnimMs_) / static_cast<float>(THROW_ANIM_MS));
  const float distanceM = static_cast<float>(throwSignedDistanceCm_) / 100.0f;
  const int travelPx = constrain(static_cast<int>(distanceM * 1.35f), -18, 48);
  const int ballX = originX + static_cast<int>(static_cast<float>(travelPx) * progress);
  const int arc = static_cast<int>(sinf(progress * 3.14159f) * 13.0f);
  const int ballY = groundY - 2 - arc;
  u8g2.drawDisc(ballX, ballY, 2);
  u8g2.drawLine(originX, groundY - 6, ballX, ballY);

  u8g2.setCursor(left_ + 38, 18);
  if (throwSignedDistanceCm_ < 0) {
    u8g2.print("-");
  }
  u8g2.print(absInt(throwSignedDistanceCm_) / 100);
  u8g2.print(".");
  u8g2.print((absInt(throwSignedDistanceCm_) / 10) % 10);
  u8g2.print("m");
}

void MicriFieldGame::drawResult(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(left_ + 3, 9, qualified_ ? "Qualified" : "No Qual");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 3, 18);
  if (resultUnit_ == ResultUnit::Time) {
    u8g2.print("Time ");
    u8g2.print(resultValue_ / 1000);
    u8g2.print(".");
    u8g2.print((resultValue_ / 100) % 10);
    u8g2.print("s");
    if (eventIndex_ == 1) {
      u8g2.print(" C");
      u8g2.print(hurdlesCleared_);
    }
  } else {
    u8g2.print(resultUnit_ == ResultUnit::Height ? "Height " : "Dist ");
    if (eventIndex_ == 3 && throwSignedDistanceCm_ < 0) {
      u8g2.print("-");
    }
    const uint16_t displayValue = eventIndex_ == 3 ? static_cast<uint16_t>(absInt(throwSignedDistanceCm_)) : resultValue_;
    u8g2.print(displayValue / 100);
    u8g2.print(".");
    u8g2.print((displayValue / 10) % 10);
    u8g2.print("m");
  }
  u8g2.setCursor(left_ + 3, 27);
  u8g2.print("Pts ");
  u8g2.print(resultScore_);
  u8g2.print(" Tot ");
  u8g2.print(totalScore_);
  u8g2.setCursor(left_ + 3, 38);
  if (qualified_) {
    u8g2.print("Tap next");
  } else if (!pressureMode_ && attemptsRemaining_ > 0) {
    u8g2.print("Tap retry A");
    u8g2.print(attemptsRemaining_);
  } else {
    u8g2.print("Tap next 0pts");
  }
}

void MicriFieldGame::drawStick(U8G2& u8g2, int x, int groundY, bool airborne) {
  const int y = groundY - 9;
  u8g2.drawCircle(x, y - 3, 2);
  u8g2.drawLine(x, y - 1, x, y + 5);
  u8g2.drawLine(x, y + 2, x + 4, y + (airborne ? 0 : 3));
  u8g2.drawLine(x, y + 2, x - 4, y + (airborne ? 1 : 4));
  u8g2.drawLine(x, y + 5, x + 4, groundY);
  u8g2.drawLine(x, y + 5, x - 3, groundY - (airborne ? 4 : 0));
}

void MicriFieldGame::drawHurdle(U8G2& u8g2, int x, int groundY, bool fallen) {
  if (fallen) {
    u8g2.drawLine(x, groundY - 1, x + 11, groundY - 5);
    u8g2.drawLine(x + 1, groundY, x + 4, groundY - 3);
    u8g2.drawLine(x + 9, groundY, x + 7, groundY - 4);
    return;
  }
  u8g2.drawLine(x, groundY, x + 2, groundY - 9);
  u8g2.drawLine(x + 12, groundY, x + 10, groundY - 9);
  u8g2.drawLine(x + 2, groundY - 9, x + 10, groundY - 9);
}

void MicriFieldGame::drawAngleFan(U8G2& u8g2, int ox, int oy, float angleDeg) {
  u8g2.drawLine(ox, oy, ox + 24, oy);
  u8g2.drawLine(ox, oy, ox + 18, oy - 18);
  const float rad = -angleDeg * 0.0174533f;
  u8g2.drawLine(ox, oy, ox + static_cast<int>(cosf(rad) * 24), oy + static_cast<int>(sinf(rad) * 24));
}

void MicriFieldGame::drawTrackSplash(U8G2& u8g2) {
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, "Micri Field");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 17, "Track and Field");
  u8g2.drawLine(3, 32, 67, 32);
  u8g2.drawLine(3, 36, 67, 36);
  drawStick(u8g2, 18, 32, false);
  drawHurdle(u8g2, 40, 32);
  u8g2.drawLine(53, 22, 64, 14);
  u8g2.drawPixel(66, 13);
}
