#include "CaveChopperGame.h"

#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
Preferences cavePrefs;

template <typename Canvas>
void drawChopperShape(Canvas& canvas, int x, int y, bool rotor) {
  if (rotor) canvas.drawFastHLine(x + 7, y - 7, 28, TFT_CYAN);
  else canvas.drawFastHLine(x + 18, y - 9, 8, TFT_CYAN);
  canvas.drawFastVLine(x + 22, y - 6, 5, TFT_CYAN);
  canvas.drawLine(x + 1, y - 3, x + 9, y + 1, TFT_YELLOW);
  canvas.drawLine(x + 1, y + 8, x + 9, y + 4, TFT_YELLOW);
  canvas.drawFastVLine(x, y - 5, 11, TFT_YELLOW);
  canvas.fillRoundRect(x + 9, y, 14, 7, 2, TFT_YELLOW);
  canvas.fillRoundRect(x + 24, y - 2, 15, 9, 2, TFT_ORANGE);
  canvas.drawFastHLine(x + 10, y + 9, 23, TFT_LIGHTGREY);
}
}

CaveChopperGame::CaveChopperGame(uint32_t width, uint32_t height)
    : App("Cave Chopper", width, height) {}

bool CaveChopperGame::hasCustomOverlay() const {
  return true;
}

void CaveChopperGame::onAppReset() {
  loadBestScore();
  y_ = static_cast<float>(height / 2);
  vy_ = 0.0f;
  speed_ = 42.0f;
  scoreMs_ = 0;
  score_ = 0;
  rotorMs_ = 0;
  rotorOn_ = false;
  lastGapTop_ = static_cast<int>(height / 2) - 7;
  randState_ ^= static_cast<uint16_t>(millis());
  for (uint8_t i = 0; i < SEGMENTS; i++) segments_[i].active = false;
  for (uint8_t i = 0; i < SEGMENTS; i++) spawnSegment(static_cast<float>(i * SEG_W));
}

void CaveChopperGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)b2;
  const float dt = static_cast<float>(deltaMs) * 0.001f;
  if (b1.down) vy_ -= 170.0f * dt;
  else vy_ += 88.0f * dt;
  if (b1.pressed) vy_ -= 10.0f;
  if (vy_ < -58.0f) vy_ = -58.0f;
  if (vy_ > 56.0f) vy_ = 56.0f;
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

  if (y_ < 20.0f || y_ > height - 10) {
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
  speed_ += 1.6f * dt;
  if (speed_ > 88.0f) speed_ = 88.0f;
  rotorMs_ += deltaMs;
  if (rotorMs_ > 90) {
    rotorMs_ = 0;
    rotorOn_ = !rotorOn_;
  }
}

void CaveChopperGame::drawRunning(TFT_eSPI& tft) {
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    TDisplayUi::clear(canvas);
    TDisplayUi::header(canvas, "Cave Chopper", TFT_GREEN, (String(score_) + "s").c_str());
    canvas.drawRect(0, TDisplayUi::HEADER_H, width, height - TDisplayUi::HEADER_H, TFT_DARKGREY);
    for (uint8_t i = 0; i < SEGMENTS; i++) {
      if (!segments_[i].active) continue;
      const int x = static_cast<int>(segments_[i].x);
      const int topH = segments_[i].gapTop;
      const int bottomY = segments_[i].gapTop + GAP_H;
      if (topH > TDisplayUi::HEADER_H) canvas.fillRect(x, TDisplayUi::HEADER_H, SEG_W, topH - TDisplayUi::HEADER_H, TFT_DARKGREEN);
      if (bottomY < static_cast<int>(height)) canvas.fillRect(x, bottomY, SEG_W, height - bottomY - 1, TFT_GREEN);
    }
    drawChopperShape(canvas, 34, static_cast<int>(y_), rotorOn_);
  });
}

void CaveChopperGame::drawChopper(TFT_eSPI& tft, int x, int y, bool rotor) const {
  drawChopperShape(tft, x, y, rotor);
}

void CaveChopperGame::drawStart(TFT_eSPI& tft) { tft.fillScreen(TFT_BLACK);
  loadBestScore();
  TDisplayUi::clear(tft);
  if (showStartPromptPage()) {
    TDisplayUi::header(tft, "Cave Chopper", TFT_GREEN);
    TDisplayUi::centered(tft, "Press to Start", 56, 2, TFT_WHITE);
    TDisplayUi::footer(tft, "B1 tap start / hold thrust in game");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    TDisplayUi::header(tft, "Top Time", TFT_GREEN);
    String score = bestScore_ == 0 ? "--" : String(initials) + " " + String(bestScore_) + "s";
    TDisplayUi::largeValue(tft, score, 54, TFT_GREEN);
    TDisplayUi::footer(tft, "Best survival time");
  } else {
    TDisplayUi::header(tft, "Cave Chopper", TFT_GREEN);
    tft.fillRect(0, 34, width, 22, TFT_DARKGREEN);
    tft.fillRect(0, 102, width, 33, TFT_GREEN);
    drawChopper(tft, 92, 75, true);
    TDisplayUi::centered(tft, "Hold to climb", 112, 1, TFT_LIGHTGREY);
  }
}

void CaveChopperGame::drawEnd(TFT_eSPI& tft) { tft.fillScreen(TFT_BLACK);
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Crashed", TFT_RED);
  TDisplayUi::labelValue(tft, 49, "Time", String(score_) + "s", TFT_YELLOW);
  String best = String(bestScore_) + "s";
  if (bestScore_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    best += " ";
    best += initials;
  }
  TDisplayUi::labelValue(tft, 78, "Best", best, TFT_GREEN);
  TDisplayUi::footer(tft, "B1 retry / B1 hold menu");
}

int CaveChopperGame::clampInt(int value, int minValue, int maxValue) const {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

int CaveChopperGame::nextGapTop() {
  randState_ = static_cast<uint16_t>(randState_ * 2053u + 13849u);
  const int step = static_cast<int>(randState_ % 5) - 2;
  return clampInt(lastGapTop_ + step * 4, TDisplayUi::HEADER_H + 8, static_cast<int>(height) - GAP_H - 8);
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
  const int px1 = 34;
  const int px2 = 68;
  const int py1 = static_cast<int>(y_) - 8;
  const int py2 = static_cast<int>(y_) + 10;
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
