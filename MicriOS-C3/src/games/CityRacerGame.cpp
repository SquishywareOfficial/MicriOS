#include "CityRacerGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
Preferences cityRacerPrefs;
constexpr int PLAYER_Y = 30;
}

CityRacerGame::CityRacerGame(uint32_t width, uint32_t height, uint32_t left)
    : App("City Racer", width, height), left_(left) {}

bool CityRacerGame::hasCustomOverlay() const {
  return true;
}

void CityRacerGame::onAppReset() {
  loadBestScore();
  playerLane_ = 1;
  plannedSafeLane_ = playerLane_;
  rowsSinceLaneChange_ = 0;
  level_ = 1;
  score_ = 0;
  speed_ = 30.0f;
  spawnIntervalMs_ = 1250;
  spawnTimerMs_ = 900;
  for (uint8_t i = 0; i < ROWS; i++) rows_[i].active = false;
}

void CityRacerGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  const float dt = static_cast<float>(deltaMs) * 0.001f;
  if (input.click) playerLane_ = (playerLane_ + 1) % LANES;

  level_ = 1 + min<uint16_t>(9, score_ / 35);
  speed_ = 30.0f + static_cast<float>(min<uint8_t>(level_ - 1, 7)) * 3.8f;
  spawnIntervalMs_ = 1250 - min<uint16_t>(420, max<int>(0, level_ - 1) * 45);

  spawnTimerMs_ += deltaMs;
  if (spawnTimerMs_ >= spawnIntervalMs_) {
    spawnTimerMs_ = 0;
    spawnRow();
  }

  for (uint8_t i = 0; i < ROWS; i++) {
    if (!rows_[i].active) continue;
    rows_[i].y += speed_ * dt;
    if (!rows_[i].counted && rows_[i].y > PLAYER_Y + 8) {
      rows_[i].counted = true;
      for (uint8_t lane = 0; lane < LANES; lane++) {
        if (rows_[i].mask & (1 << lane)) score_++;
      }
      if (score_ > bestScore_) {
        bestScore_ = score_;
        saveBestScore();
      }
    }
    if (rows_[i].y > height + 8) rows_[i].active = false;
    if (rows_[i].y >= PLAYER_Y - 7 && rows_[i].y <= PLAYER_Y + 7 && (rows_[i].mask & (1 << playerLane_))) {
      endApp();
      return;
    }
  }
}

uint8_t CityRacerGame::chooseSafeLane() {
  if (rowsSinceLaneChange_ == 0) {
    rowsSinceLaneChange_++;
    return plannedSafeLane_;
  }
  const uint8_t choice = random(0, 100);
  if (rowsSinceLaneChange_ < 3 && choice < 52) {
    rowsSinceLaneChange_++;
    return plannedSafeLane_;
  }
  rowsSinceLaneChange_ = 0;
  return (plannedSafeLane_ + 1) % LANES;
}

void CityRacerGame::spawnRow() {
  for (uint8_t i = 0; i < ROWS; i++) {
    if (rows_[i].active && rows_[i].y < 15.0f) return;
  }
  for (uint8_t i = 0; i < ROWS; i++) {
    if (rows_[i].active) continue;
    plannedSafeLane_ = chooseSafeLane();
    uint8_t mask = 0;
    const uint8_t firstBlocked = (plannedSafeLane_ + (random(0, 100) < 55 ? 1 : 2)) % LANES;
    mask |= (1 << firstBlocked);
    const bool addSecondCar = level_ >= 3 && random(0, 100) < min<uint8_t>(48, 18 + (level_ - 3) * 5);
    if (addSecondCar) {
      const uint8_t secondBlocked = (plannedSafeLane_ + 3 - (firstBlocked - plannedSafeLane_ + 3) % 3) % LANES;
      mask |= (1 << secondBlocked);
    }
    rows_[i].active = true;
    rows_[i].counted = false;
    rows_[i].y = -8.0f;
    rows_[i].mask = mask;
    return;
  }
}

void CityRacerGame::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  u8g2.drawLine(left_ + 23, 1, left_ + 23, height - 2);
  u8g2.drawLine(left_ + 46, 1, left_ + 46, height - 2);
  for (uint8_t i = 0; i < ROWS; i++) {
    if (!rows_[i].active) continue;
    for (uint8_t lane = 0; lane < LANES; lane++) {
      if (rows_[i].mask & (1 << lane)) drawCar(u8g2, left_ + laneX(lane), static_cast<int>(rows_[i].y), false);
    }
  }
  drawCar(u8g2, left_ + laneX(playerLane_), PLAYER_Y, true);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 2, 6);
  u8g2.print("L");
  u8g2.print(level_);
  u8g2.print(" ");
  u8g2.print(score_);
}

void CityRacerGame::drawCar(U8G2& u8g2, int x, int y, bool player) const {
  if (player) {
    u8g2.drawFrame(x + 1, y, 6, 8);
    u8g2.drawBox(x + 2, y + 1, 4, 2);
  } else {
    u8g2.drawBox(x + 1, y, 6, 8);
    u8g2.setDrawColor(0);
    u8g2.drawPixel(x + 2, y + 1);
    u8g2.drawPixel(x + 5, y + 1);
    u8g2.setDrawColor(1);
  }
}

void CityRacerGame::drawStart(U8G2& u8g2) {
  loadBestScore();
  u8g2.drawFrame(0, 0, width + 2, height);
  if (showStartPromptPage()) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(20, 16, "Press");
    u8g2.drawStr(13, 29, "to Start");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 10, "Top Cars");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(3, 24);
    if (bestScore_ == 0) u8g2.print("--");
    else {
      u8g2.print(initials);
      u8g2.print(" ");
      u8g2.print(bestScore_);
    }
  } else {
    u8g2.drawLine(18, 11, 7, 36);
    u8g2.drawLine(54, 11, 66, 36);
    u8g2.drawLine(32, 9, 30, 36);
    u8g2.drawLine(40, 9, 42, 36);
    drawCar(u8g2, 31, 22, true);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 9, appTitle());
  }
}

void CityRacerGame::drawEnd(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, "Crashed");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(3, 20);
  u8g2.print("Cars ");
  u8g2.print(score_);
  u8g2.setCursor(3, 29);
  u8g2.print("Best ");
  u8g2.print(bestScore_);
  if (bestScore_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.print(" ");
    u8g2.print(initials);
  }
  u8g2.drawStr(3, 38, "Tap retry Hold menu");
}

int CityRacerGame::laneX(uint8_t lane) const {
  return 7 + lane * 23;
}

void CityRacerGame::loadBestScore() {
  if (bestLoaded_) return;
  cityRacerPrefs.begin("cityrace", true);
  bestScore_ = cityRacerPrefs.getUShort("best", 0);
  bestInitials_ = cityRacerPrefs.getUShort("init", PlayerProfile::defaultInitials());
  cityRacerPrefs.end();
  bestLoaded_ = true;
}

void CityRacerGame::saveBestScore() {
  bestInitials_ = PlayerProfile::loadInitials();
  cityRacerPrefs.begin("cityrace", false);
  cityRacerPrefs.putUShort("best", bestScore_);
  cityRacerPrefs.putUShort("init", bestInitials_);
  cityRacerPrefs.end();
}
