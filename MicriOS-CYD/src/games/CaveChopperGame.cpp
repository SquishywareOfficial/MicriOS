#include "CaveChopperGame.h"

#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../CydFramebuffer.h"
#include "../../CydUi.h"

namespace {
Preferences cavePrefs;
constexpr float START_SPEED = 76.0f;
constexpr float MAX_SPEED = 145.0f;
constexpr float SPEED_GAIN = 2.8f;
constexpr float GRAVITY = 182.0f;
constexpr float THRUST = 365.0f;
constexpr float PRESS_IMPULSE = 14.0f;
constexpr float MAX_RISE_SPEED = -108.0f;
constexpr float MAX_FALL_SPEED = 126.0f;
constexpr uint8_t MAX_OBSTACLE_DEPTH = 27;

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
  speed_ = START_SPEED;
  scoreMs_ = 0;
  score_ = 0;
  rotorMs_ = 0;
  rotorOn_ = false;
  lastGapTop_ = static_cast<int>(height / 2) - GAP_H / 2;
  obstacleCooldown_ = 10;
  obstacleRemaining_ = 0;
  obstacleSpan_ = 0;
  obstacleFromTop_ = false;
  randState_ ^= static_cast<uint16_t>(millis());
  for (uint8_t i = 0; i < SEGMENTS; i++) segments_[i].active = false;
  initializingSegments_ = true;
  for (uint8_t i = 0; i < SEGMENTS; i++) spawnSegment(static_cast<float>(i * SEG_W));
  initializingSegments_ = false;
}

void CaveChopperGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)b2;
  const float dt = static_cast<float>(deltaMs) * 0.001f;
  vy_ += (b1.down ? GRAVITY - THRUST : GRAVITY) * dt;
  if (b1.pressed) vy_ -= PRESS_IMPULSE;
  if (vy_ < MAX_RISE_SPEED) vy_ = MAX_RISE_SPEED;
  if (vy_ > MAX_FALL_SPEED) vy_ = MAX_FALL_SPEED;
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

  if (y_ < CydUi::HEADER_H + 8.0f || y_ > height - 10.0f) {
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
  speed_ += SPEED_GAIN * dt;
  if (speed_ > MAX_SPEED) speed_ = MAX_SPEED;
  rotorMs_ += deltaMs;
  if (rotorMs_ > 90) {
    rotorMs_ = 0;
    rotorOn_ = !rotorOn_;
  }
}

void CaveChopperGame::drawRunning(TFT_eSPI& tft) {
  CydFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    CydUi::clear(canvas);
    CydUi::header(canvas, "Cave Chopper", TFT_GREEN, (String(score_) + "s").c_str());
    canvas.drawRect(0, CydUi::HEADER_H, width, height - CydUi::HEADER_H, TFT_DARKGREY);
    for (uint8_t i = 0; i < SEGMENTS; i++) {
      if (!segments_[i].active) continue;
      const int x = static_cast<int>(segments_[i].x);
      const int topH = segments_[i].gapTop +
                       (segments_[i].obstacleFromTop ? segments_[i].obstacleDepth : 0);
      const int bottomY = segments_[i].gapTop + GAP_H -
                          (segments_[i].obstacleFromTop ? 0 : segments_[i].obstacleDepth);
      if (topH > CydUi::HEADER_H) canvas.fillRect(x, CydUi::HEADER_H, SEG_W, topH - CydUi::HEADER_H, TFT_DARKGREEN);
      if (bottomY < static_cast<int>(height)) canvas.fillRect(x, bottomY, SEG_W, height - bottomY - 1, TFT_GREEN);
      if (segments_[i].obstacleDepth > 0) {
        const int tipY = segments_[i].obstacleFromTop ? topH - 2 : bottomY;
        canvas.drawFastHLine(x, tipY, SEG_W, TFT_YELLOW);
      }
    }
    drawChopperShape(canvas, 45, static_cast<int>(y_), rotorOn_);
  });
}

void CaveChopperGame::drawChopper(TFT_eSPI& tft, int x, int y, bool rotor) const {
  drawChopperShape(tft, x, y, rotor);
}

void CaveChopperGame::drawStart(TFT_eSPI& tft) {
  loadBestScore();
  const bool promptPage = showStartPromptPage();
  const bool scorePage = showStartScorePage();
  CydFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    CydUi::clear(canvas);
    if (promptPage) {
      CydUi::header(canvas, "Cave Chopper", TFT_GREEN);
      CydUi::centered(canvas, "Press to Start", 56, 2, TFT_WHITE);
      CydUi::footer(canvas, "Hold the screen for thrust");
    } else if (scorePage) {
      char initials[4];
      PlayerProfile::unpackDottedInitials(bestInitials_, initials);
      CydUi::header(canvas, "Top Time", TFT_GREEN);
      String score = bestScore_ == 0 ? "--" : String(initials) + " " + String(bestScore_) + "s";
      CydUi::largeValue(canvas, score, 54, TFT_GREEN);
      CydUi::footer(canvas, "Best survival time");
    } else {
      CydUi::header(canvas, "Cave Chopper", TFT_GREEN);
      canvas.fillRect(0, 34, width, 34, TFT_DARKGREEN);
      canvas.fillRect(0, 142, width, height - 142, TFT_GREEN);
      drawChopperShape(canvas, 128, 93, true);
      CydUi::centered(canvas, "Hold to climb", 118, 2, TFT_LIGHTGREY);
    }
  });
}

void CaveChopperGame::drawEnd(TFT_eSPI& tft) {
  CydFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    CydUi::clear(canvas);
    CydUi::header(canvas, "Crashed", TFT_RED);
    CydUi::labelValue(canvas, 49, "Time", String(score_) + "s", TFT_YELLOW);
    String best = String(bestScore_) + "s";
    if (bestScore_ > 0) {
      char initials[4];
      PlayerProfile::unpackDottedInitials(bestInitials_, initials);
      best += " ";
      best += initials;
    }
    CydUi::labelValue(canvas, 78, "Best", best, TFT_GREEN);
    CydUi::footer(canvas, "Tap Play Again");
  });
}

uint16_t CaveChopperGame::staticRenderIntervalMs() const {
  return phase() == AppPhase::Start ? 2000 : 0;
}

uint16_t CaveChopperGame::phaseActionColor() const {
  return TFT_ORANGE;
}

int CaveChopperGame::clampInt(int value, int minValue, int maxValue) const {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

int CaveChopperGame::nextGapTop() {
  randState_ = static_cast<uint16_t>(randState_ * 2053u + 13849u);
  const int step = static_cast<int>(randState_ % 5) - 2;
  return clampInt(lastGapTop_ + step * 5, CydUi::HEADER_H + 5, static_cast<int>(height) - GAP_H - 5);
}

void CaveChopperGame::spawnSegment(float x) {
  for (uint8_t i = 0; i < SEGMENTS; i++) {
    if (!segments_[i].active) {
      lastGapTop_ = nextGapTop();
      segments_[i].x = x;
      segments_[i].gapTop = lastGapTop_;
      segments_[i].obstacleDepth = 0;
      segments_[i].obstacleFromTop = false;

      if (!initializingSegments_) {
        if (obstacleRemaining_ == 0) {
          if (obstacleCooldown_ > 0) {
            obstacleCooldown_--;
          } else {
            randState_ = static_cast<uint16_t>(randState_ * 2053u + 13849u);
            obstacleSpan_ = 3 + randState_ % 2;
            obstacleRemaining_ = obstacleSpan_;
            obstacleFromTop_ = (randState_ & 0x80u) != 0;
          }
        }
        if (obstacleRemaining_ > 0) {
          const uint8_t progress = obstacleSpan_ - obstacleRemaining_;
          const uint8_t edge = min<uint8_t>(progress + 1, obstacleRemaining_);
          segments_[i].obstacleDepth = min<uint8_t>(MAX_OBSTACLE_DEPTH,
                                                    static_cast<uint8_t>(11 + edge * 8));
          segments_[i].obstacleFromTop = obstacleFromTop_;
          obstacleRemaining_--;
          if (obstacleRemaining_ == 0) {
            randState_ = static_cast<uint16_t>(randState_ * 2053u + 13849u);
            obstacleCooldown_ = 12 + randState_ % 13;
          }
        }
      }
      segments_[i].active = true;
      return;
    }
  }
}

bool CaveChopperGame::collides(const Segment& segment) const {
  const int px1 = 45;
  const int px2 = 84;
  const int py1 = static_cast<int>(y_) - 8;
  const int py2 = static_cast<int>(y_) + 10;
  const int sx1 = static_cast<int>(segment.x);
  const int sx2 = sx1 + SEG_W - 1;
  if (px2 < sx1 || px1 > sx2) return false;
  const int safeTop = segment.gapTop +
                      (segment.obstacleFromTop ? segment.obstacleDepth : 0);
  const int safeBottom = segment.gapTop + GAP_H -
                         (segment.obstacleFromTop ? 0 : segment.obstacleDepth);
  return py1 < safeTop || py2 > safeBottom;
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
