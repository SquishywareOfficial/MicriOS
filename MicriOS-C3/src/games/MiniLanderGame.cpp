#include "MiniLanderGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
constexpr int HUD_H = 8;
constexpr int GROUND_Y = 36;
constexpr int SHIP_X = 35;
constexpr int LANDER_FOOT_OFFSET = 5;
constexpr uint8_t BURN_HINT_LED_PIN = 8;
constexpr bool BURN_HINT_LED_ACTIVE_LOW = true;
constexpr float THRUST = 48.0f;
constexpr float FUEL_BURN = 24.0f;
constexpr float BURN_CUE_REACTION_SEC = 0.35f;
constexpr uint16_t BRIEFING_INPUT_LOCK_MS = 1000;
Preferences landerPrefs;
}

MiniLanderGame::MiniLanderGame(uint32_t width, uint32_t height, uint32_t left)
    : App("Mini Lander", width, height), left_(left) {}

bool MiniLanderGame::hasCustomOverlay() const {
  return true;
}

void MiniLanderGame::onAppReset() {
  pinMode(BURN_HINT_LED_PIN, OUTPUT);
  setBurnHintLed(false);
  loadBestLevel();
  level_ = 1;
  crashed_ = false;
  loadLevel();
}

void MiniLanderGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  const float deltaSec = static_cast<float>(deltaMs) * 0.001f;

  if (landerState_ == LanderState::Briefing) {
    setBurnHintLed(false);
    briefingTimerMs_ += deltaMs;
    if (briefingTimerMs_ < BRIEFING_INPUT_LOCK_MS) {
      return;
    }
    if (!briefingCanAcceptInput_) {
      if (!input.down) {
        briefingCanAcceptInput_ = true;
      }
      return;
    }
    if (input.click) {
      landerState_ = LanderState::Falling;
    }
    return;
  }

  if (landerState_ == LanderState::Landed) {
    setBurnHintLed(false);
    landedTimerMs_ += deltaMs;
    if (landedTimerMs_ >= 900) {
      level_++;
      loadLevel();
    }
    return;
  }

  const bool thrusting = input.down && fuel_ > 0.0f;
  thrusting_ = thrusting;
  const bool lowFuelBlink = fuel_ <= (startFuel_ * 0.2f) && ((millis() / 180) % 2) == 0;
  setBurnHintLed((shouldShowBurnHint() && !thrusting) || lowFuelBlink);
  float accel = levelGravity();
  if (thrusting) {
    accel -= THRUST;
    fuel_ -= FUEL_BURN * deltaSec;
    if (fuel_ < 0.0f) {
      fuel_ = 0.0f;
    }
  }

  velocity_ += accel * deltaSec;
  altitude_ -= velocity_ * deltaSec;

  if (altitude_ <= 0.0f) {
    altitude_ = 0.0f;
    if (velocity_ <= levelSafeSpeed()) {
      velocity_ = 0.0f;
      landerState_ = LanderState::Landed;
      landedTimerMs_ = 0;
      recordLandedLevel();
      setBurnHintLed(false);
    } else {
      crashed_ = true;
      setBurnHintLed(false);
      endApp();
    }
  }
}

void MiniLanderGame::drawRunning(U8G2& u8g2) {
  if (landerState_ == LanderState::Briefing) {
    drawBriefing(u8g2);
    return;
  }

  u8g2.drawFrame(left_, 0, width, height);
  drawHud(u8g2);
  drawGround(u8g2);

  const float altitudeRatio = altitude_ / startAltitude_;
  const int altitudePixels = altitude_ <= 0.0f ? 0 : max(1, static_cast<int>(ceilf(altitudeRatio * 22.0f)));
  const int shipY = GROUND_Y - LANDER_FOOT_OFFSET - altitudePixels;
  drawLander(u8g2, left_ + SHIP_X, shipY, thrusting_);

  if (landerState_ == LanderState::Landed) {
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(left_ + 21, 24, "LANDED");
  }
}

void MiniLanderGame::drawStart(U8G2& u8g2) {
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
    u8g2.drawStr(3, 10, "Best Level");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(3, 24);
    if (bestLevel_ == 0) {
      u8g2.print("--");
    } else {
      u8g2.print(initials);
      u8g2.print(" L");
      u8g2.print(bestLevel_);
    }
    return;
  }
  u8g2.drawCircle(17, 20, 12);
  u8g2.drawCircle(12, 16, 2);
  u8g2.drawCircle(20, 23, 3);
  u8g2.drawCircle(23, 14, 1);
  u8g2.drawLine(42, 9, 50, 13);
  u8g2.drawTriangle(52, 14, 57, 11, 55, 18);
  u8g2.drawPixel(39, 8);
  u8g2.drawPixel(61, 22);
  u8g2.drawPixel(49, 30);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 36, appTitle());
}

void MiniLanderGame::drawEnd(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, crashed_ ? "Crashed" : "Mission End");
  u8g2.setCursor(3, 21);
  u8g2.print("Level:");
  u8g2.print(level_);
  u8g2.setCursor(3, 32);
  u8g2.print("V:");
  u8g2.print(static_cast<int>(velocity_ + 0.5f));
  drawSafeSpeedSign(u8g2, 57, 25, static_cast<int>(levelSafeSpeed() + 0.5f));
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 39, "Tap retry Hold menu");
}

void MiniLanderGame::loadBestLevel() {
  if (bestLoaded_) {
    return;
  }
  landerPrefs.begin("lander", true);
  bestLevel_ = landerPrefs.getUChar("best", 0);
  bestInitials_ = landerPrefs.getUShort("init", PlayerProfile::defaultInitials());
  landerPrefs.end();
  bestLoaded_ = true;
}

void MiniLanderGame::saveBestLevel() {
  bestInitials_ = PlayerProfile::loadInitials();
  landerPrefs.begin("lander", false);
  landerPrefs.putUChar("best", bestLevel_);
  landerPrefs.putUShort("init", bestInitials_);
  landerPrefs.end();
}

void MiniLanderGame::recordLandedLevel() {
  loadBestLevel();
  if (level_ > bestLevel_) {
    bestLevel_ = level_;
    saveBestLevel();
  }
}

void MiniLanderGame::loadLevel() {
  landerState_ = LanderState::Briefing;
  briefingTimerMs_ = 0;
  briefingCanAcceptInput_ = false;
  landedTimerMs_ = 0;
  startAltitude_ = 90.0f + static_cast<float>(level_ - 1) * 8.0f;
  altitude_ = startAltitude_;
  velocity_ = 3.5f + static_cast<float>(level_ - 1) * 1.6f;
  startFuel_ = max(22.0f, 70.0f - static_cast<float>(level_ - 1) * 6.0f);
  fuel_ = startFuel_;
  thrusting_ = false;
  setBurnHintLed(false);
}

float MiniLanderGame::levelGravity() const {
  if (level_ <= 3) {
    return 6.5f + static_cast<float>(level_ - 1) * 1.4f;
  }
  return 11.5f + static_cast<float>(level_ - 4) * 2.1f;
}

float MiniLanderGame::levelSafeSpeed() const {
  if (level_ == 1) {
    return 14.0f;
  }
  if (level_ == 2) {
    return 13.0f;
  }
  if (level_ == 3) {
    return 12.0f;
  }
  return max(8.0f, 12.0f - static_cast<float>(level_ - 3));
}

bool MiniLanderGame::shouldShowBurnHint() const {
  if (level_ > 3 || landerState_ != LanderState::Falling || fuel_ <= 0.0f) {
    return false;
  }
  const float netDecel = THRUST - levelGravity();
  if (netDecel <= 0.0f || velocity_ <= 1.0f) {
    return false;
  }
  const float gravity = levelGravity();
  const float reactionDistance =
      (velocity_ * BURN_CUE_REACTION_SEC) + (0.5f * gravity * BURN_CUE_REACTION_SEC * BURN_CUE_REACTION_SEC);
  const float velocityAfterReaction = velocity_ + (gravity * BURN_CUE_REACTION_SEC);
  const float stoppingDistance = (velocityAfterReaction * velocityAfterReaction) / (2.0f * netDecel);
  const float cueError = altitude_ - (reactionDistance + stoppingDistance);
  return cueError <= 3.0f && cueError >= -2.0f;
}

void MiniLanderGame::setBurnHintLed(bool on) {
  digitalWrite(BURN_HINT_LED_PIN, BURN_HINT_LED_ACTIVE_LOW ? !on : on);
}

void MiniLanderGame::drawSafeSpeedSign(U8G2& u8g2, int x, int y, int safeSpeed) {
  u8g2.drawCircle(x, y, 8);
  u8g2.drawCircle(x, y, 7);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(x - (safeSpeed >= 10 ? 5 : 2), y + 3);
  u8g2.print(safeSpeed);
}

void MiniLanderGame::drawBriefing(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(3, 8);
  u8g2.print("Level ");
  u8g2.print(level_);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(3, 17);
  u8g2.print("Fuel ");
  u8g2.print(static_cast<int>(startFuel_ + 0.5f));
  u8g2.print("  G ");
  u8g2.print(static_cast<int>(levelGravity() + 0.5f));
  u8g2.setCursor(3, 26);
  u8g2.print("Safe V <= ");
  u8g2.print(static_cast<int>(levelSafeSpeed() + 0.5f));
  u8g2.setCursor(3, 37);
  if (briefingTimerMs_ < BRIEFING_INPUT_LOCK_MS || !briefingCanAcceptInput_) {
    u8g2.print("Ready...");
  } else {
    u8g2.print("Tap start");
  }
}

void MiniLanderGame::drawHud(U8G2& u8g2) {
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 2, 6);
  u8g2.print("L");
  u8g2.print(level_);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(left_ + 14, 8);
  u8g2.print(" A");
  u8g2.print(static_cast<int>(altitude_ + 0.5f));
  u8g2.print(" V");
  u8g2.print(static_cast<int>(velocity_ + 0.5f));

  const int fuelH = static_cast<int>(22.0f * (fuel_ / startFuel_));
  u8g2.drawFrame(left_ + 66, 10, 3, 24);
  if (fuelH > 0) {
    u8g2.drawBox(left_ + 67, 33 - fuelH, 1, fuelH);
  }
}

void MiniLanderGame::drawLander(U8G2& u8g2, int x, int y, bool thrusting) {
  u8g2.drawTriangle(x, y - 4, x - 3, y + 2, x + 3, y + 2);
  u8g2.drawLine(x - 3, y + 2, x - 5, y + 5);
  u8g2.drawLine(x + 3, y + 2, x + 5, y + 5);
  if (thrusting) {
    u8g2.drawLine(x - 1, y + 3, x, y + 8);
    u8g2.drawLine(x + 1, y + 3, x, y + 8);
  }
}

void MiniLanderGame::drawGround(U8G2& u8g2) {
  u8g2.drawLine(left_ + 1, GROUND_Y, left_ + width - 2, GROUND_Y);
  u8g2.drawBox(left_ + 27, GROUND_Y - 1, 18, 2);
  u8g2.drawPixel(left_ + 8, GROUND_Y - 2);
  u8g2.drawPixel(left_ + 55, GROUND_Y - 3);
}
