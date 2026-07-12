#include "CaveChopperGame.h"

#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
Preferences cavePrefs;
}

CaveChopperGame::CaveChopperGame(uint32_t width, uint32_t height, uint32_t left)
    : App("Cave Chopper", width, height), left_(left) {}

bool CaveChopperGame::hasCustomOverlay() const {
  return true;
}

void CaveChopperGame::onAppReset() {
  loadBestScore();
  y_ = 18.0f;
  vy_ = 0.0f;
  speed_ = 13.0f;
  scoreMs_ = 0;
  score_ = 0;
  rotorMs_ = 0;
  rotorOn_ = false;
  lastGapTop_ = static_cast<int>(height / 2) - 7;
  randState_ ^= static_cast<uint16_t>(millis());
  for (uint8_t i = 0; i < SEGMENTS; i++) segments_[i].active = false;
  for (uint8_t i = 0; i < SEGMENTS; i++) spawnSegment(static_cast<float>(i * SEG_W));
}

void CaveChopperGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  const float dt = static_cast<float>(deltaMs) * 0.001f;
  if (input.down) vy_ -= 72.0f * dt;
  else vy_ += 36.0f * dt;
  if (input.pressed) vy_ -= 5.0f;
  if (vy_ < -20.0f) vy_ = -20.0f;
  if (vy_ > 19.0f) vy_ = 19.0f;
  y_ += vy_ * dt;

  float rightMost = -1000.0f;
  for (uint8_t i = 0; i < SEGMENTS; i++) {
    if (!segments_[i].active) continue;
    segments_[i].x -= speed_ * dt;
    if (segments_[i].x > rightMost) rightMost = segments_[i].x;
    if (segments_[i].x + SEG_W < 0) segments_[i].active = false;
  }
  for (uint8_t i = 0; i < SEGMENTS; i++) {
    if (!segments_[i].active) {
      spawnSegment(rightMost + SEG_W);
      if (segments_[i].x > rightMost) rightMost = segments_[i].x;
    }
  }

  if (y_ < 1.0f || y_ > height - 6) {
    endApp();
    return;
  }
  for (uint8_t i = 0; i < SEGMENTS; i++) {
    if (segments_[i].active && collides(segments_[i])) {
      if (score_ > bestScore_) {
        bestScore_ = score_;
        saveBestScore();
      }
      endApp();
      return;
    }
  }

  scoreMs_ += deltaMs;
  score_ = scoreMs_ / 1000;
  if (score_ > bestScore_) {
    bestScore_ = score_;
    saveBestScore();
  }
  speed_ += 0.45f * dt;
  if (speed_ > 32.0f) speed_ = 32.0f;
  rotorMs_ += deltaMs;
  if (rotorMs_ > 90) {
    rotorMs_ = 0;
    rotorOn_ = !rotorOn_;
  }
}

void CaveChopperGame::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  for (uint8_t i = 0; i < SEGMENTS; i++) {
    if (!segments_[i].active) continue;
    const int x = static_cast<int>(segments_[i].x);
    const int topH = segments_[i].gapTop;
    const int bottomY = segments_[i].gapTop + GAP_H;
    if (topH > 0) u8g2.drawBox(left_ + x, 1, SEG_W, topH);
    if (bottomY < static_cast<int>(height)) u8g2.drawBox(left_ + x, bottomY, SEG_W, height - bottomY - 1);
  }
  drawChopper(u8g2, left_ + 12, static_cast<int>(y_), rotorOn_);
  u8g2.setDrawColor(0);
  u8g2.drawBox(left_ + 1, 1, 18, 7);
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 2, 7);
  u8g2.print(score_);
}

void CaveChopperGame::drawChopper(U8G2& u8g2, int x, int y, bool rotor) const {
  if (rotor) u8g2.drawLine(x + 1, y - 2, x + 10, y - 2);
  else u8g2.drawLine(x + 5, y - 3, x + 7, y - 3);
  u8g2.drawLine(x + 6, y - 2, x + 6, y);
  u8g2.drawBox(x, y + 1, 4, 3);
  u8g2.drawBox(x + 6, y, 6, 4);
  u8g2.drawPixel(x + 12, y + 2);
  u8g2.drawLine(x + 2, y + 4, x + 10, y + 4);
}

void CaveChopperGame::drawStart(U8G2& u8g2) {
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
    u8g2.drawStr(3, 10, "Top Time");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(3, 24);
    if (bestScore_ == 0) u8g2.print("--");
    else {
      u8g2.print(initials);
      u8g2.print(" ");
      u8g2.print(bestScore_);
      u8g2.print("s");
    }
  } else {
    u8g2.drawLine(1, 12, 71, 12);
    u8g2.drawLine(1, 34, 71, 34);
    drawChopper(u8g2, 23, 22, true);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 8, appTitle());
  }
}

void CaveChopperGame::drawEnd(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, "Crashed");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(3, 20);
  u8g2.print("Time ");
  u8g2.print(score_);
  u8g2.print("s");
  u8g2.setCursor(3, 29);
  u8g2.print("Best ");
  u8g2.print(bestScore_);
  u8g2.print("s");
  if (bestScore_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.print(" ");
    u8g2.print(initials);
  }
  u8g2.drawStr(3, 38, "Tap retry Hold menu");
}

int CaveChopperGame::clampInt(int value, int minValue, int maxValue) const {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

int CaveChopperGame::nextGapTop() {
  randState_ = static_cast<uint16_t>(randState_ * 2053u + 13849u);
  const int step = static_cast<int>(randState_ % 5) - 2;
  return clampInt(lastGapTop_ + step, 3, static_cast<int>(height) - GAP_H - 3);
}

void CaveChopperGame::spawnSegment(float x) {
  for (uint8_t i = 0; i < SEGMENTS; i++) {
    if (!segments_[i].active) {
      lastGapTop_ = nextGapTop();
      segments_[i].x = x;
      segments_[i].gapTop = lastGapTop_;
      segments_[i].active = true;
      return;
    }
  }
}

bool CaveChopperGame::collides(const Segment& segment) const {
  const int px1 = 12;
  const int px2 = 24;
  const int py1 = static_cast<int>(y_) - 2;
  const int py2 = static_cast<int>(y_) + 4;
  const int sx1 = static_cast<int>(segment.x);
  const int sx2 = sx1 + SEG_W - 1;
  if (px2 < sx1 || px1 > sx2) return false;
  return py1 < segment.gapTop || py2 > segment.gapTop + GAP_H;
}

void CaveChopperGame::loadBestScore() {
  if (bestLoaded_) return;
  cavePrefs.begin("cavechop", true);
  bestScore_ = cavePrefs.getUShort("best", 0);
  bestInitials_ = cavePrefs.getUShort("init", PlayerProfile::defaultInitials());
  cavePrefs.end();
  bestLoaded_ = true;
}

void CaveChopperGame::saveBestScore() {
  bestInitials_ = PlayerProfile::loadInitials();
  cavePrefs.begin("cavechop", false);
  cavePrefs.putUShort("best", bestScore_);
  cavePrefs.putUShort("init", bestInitials_);
  cavePrefs.end();
}
