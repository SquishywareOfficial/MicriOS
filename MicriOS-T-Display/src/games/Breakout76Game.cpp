#include "Breakout76Game.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
Preferences breakout76Prefs;
constexpr uint8_t BRICK_W = 23;
constexpr uint8_t BRICK_H = 9;
constexpr uint8_t BRICK_X = 17;
constexpr uint8_t BRICK_Y = 34;
constexpr uint8_t PADDLE_Y = 121;
}

Breakout76Game::Breakout76Game(uint32_t width, uint32_t height)
    : App("Breakout '76", width, height) {}

bool Breakout76Game::hasCustomOverlay() const {
  return true;
}

bool Breakout76Game::consumesButton2HoldInRunning() const {
  return true;
}

void Breakout76Game::onAppReset() {
  loadBestScore();
  paddleX_ = 99.0f;
  paddleVel_ = 0.0f;
  paddleDir_ = 1;
  paddleW_ = 42;
  wideTimerMs_ = 0;
  level_ = 1;
  score_ = 0;
  for (uint8_t i = 0; i < BALLS; i++) balls_[i].active = false;
  for (uint8_t i = 0; i < POWERUPS; i++) powers_[i].active = false;
  buildLevel();
}

void Breakout76Game::buildLevel() {
  bricksLeft_ = 0;
  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      bool on = true;
      if ((level_ % 3) == 2) on = ((r + c) % 2) == 0 || r == 0;
      if ((level_ % 3) == 0) on = (r == 0 || r == ROWS - 1 || c == 0 || c == COLS - 1 || r == c / 2);
      bricks_[r][c] = on;
      if (on) bricksLeft_++;
    }
  }
  paddleX_ = static_cast<float>((width - paddleW_) / 2);
  for (uint8_t i = 0; i < BALLS; i++) balls_[i].active = false;
  launchBall(0, false);
}

void Breakout76Game::launchBall(uint8_t slot, bool alternate) {
  if (slot >= BALLS) return;
  balls_[slot].active = true;
  balls_[slot].x = 55.0f + static_cast<float>(random(0, 125));
  balls_[slot].y = 92.0f + static_cast<float>(random(0, 8));
  const float speed = 68.0f + static_cast<float>(level_) * 4.0f;
  balls_[slot].vx = (alternate ? -1.0f : 1.0f) * (38.0f + static_cast<float>(random(0, 23)));
  balls_[slot].vy = -speed;
}

uint8_t Breakout76Game::activeBallCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < BALLS; i++) if (balls_[i].active) count++;
  return count;
}

void Breakout76Game::spawnPower(float x, float y) {
  if (random(0, 100) > 13) return;
  for (uint8_t i = 0; i < POWERUPS; i++) {
    if (!powers_[i].active) {
      powers_[i].active = true;
      powers_[i].x = x;
      powers_[i].y = y;
      powers_[i].type = random(0, 2);
      return;
    }
  }
}

void Breakout76Game::applyPower(uint8_t type) {
  if (type == 0) {
    paddleW_ = 64;
    wideTimerMs_ = 9000;
    return;
  }
  for (uint8_t i = 0; i < BALLS; i++) {
    if (!balls_[i].active) {
      launchBall(i, (i % 2) == 0);
      return;
    }
  }
}

void Breakout76Game::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  const float dt = static_cast<float>(deltaMs) * 0.001f;
  const int8_t inputDir = static_cast<int8_t>(b1.down ? 1 : 0) - static_cast<int8_t>(b2.down ? 1 : 0);
  paddleDir_ = inputDir;

  if (wideTimerMs_ > 0) {
    wideTimerMs_ = deltaMs >= wideTimerMs_ ? 0 : wideTimerMs_ - deltaMs;
    if (wideTimerMs_ == 0) paddleW_ = 42;
  }

  const float oldPaddle = paddleX_;
  paddleX_ += static_cast<float>(inputDir) * 118.0f * dt;
  if (paddleX_ < 1.0f) {
    paddleX_ = 1.0f;
  }
  if (paddleX_ > static_cast<float>(width - paddleW_ - 1)) {
    paddleX_ = static_cast<float>(width - paddleW_ - 1);
  }
  paddleVel_ = (paddleX_ - oldPaddle) / max(dt, 0.001f);

  for (uint8_t p = 0; p < POWERUPS; p++) {
    if (!powers_[p].active) continue;
    powers_[p].y += 42.0f * dt;
    if (powers_[p].y >= PADDLE_Y - 1 && powers_[p].x >= paddleX_ && powers_[p].x <= paddleX_ + paddleW_) {
      applyPower(powers_[p].type);
      powers_[p].active = false;
    } else if (powers_[p].y > height) {
      powers_[p].active = false;
    }
  }

  for (uint8_t i = 0; i < BALLS; i++) {
    if (!balls_[i].active) continue;
    Ball& b = balls_[i];
    b.x += b.vx * dt;
    b.y += b.vy * dt;

    if (b.x <= 1.0f || b.x >= width - 2) {
      b.x = b.x <= 1.0f ? 1.0f : static_cast<float>(width - 2);
      b.vx = -b.vx;
    }
    if (b.y <= 1.0f) {
      b.y = 1.0f;
      b.vy = -b.vy;
    }

    if (b.y >= PADDLE_Y - 2 && b.y <= PADDLE_Y + 1 && b.x >= paddleX_ && b.x <= paddleX_ + paddleW_ && b.vy > 0) {
      const float hit = ((b.x - paddleX_) / static_cast<float>(paddleW_)) - 0.5f;
      b.vx = hit * 115.0f + paddleVel_ * 0.22f;
      b.vy = -(70.0f + static_cast<float>(level_) * 4.0f);
      b.y = PADDLE_Y - 4;
    }

    const int bx = static_cast<int>(b.x);
    const int by = static_cast<int>(b.y);
    bool hitBrick = false;
    for (uint8_t r = 0; r < ROWS && !hitBrick; r++) {
      for (uint8_t c = 0; c < COLS; c++) {
        if (!bricks_[r][c]) continue;
        const int x = BRICK_X + c * BRICK_W;
        const int y = BRICK_Y + r * BRICK_H;
        if (bx >= x && bx < x + BRICK_W - 2 && by >= y && by < y + BRICK_H - 2) {
          bricks_[r][c] = false;
          bricksLeft_--;
          score_++;
          if (score_ > bestScore_) {
            bestScore_ = score_;
            saveBestScore();
          }
          spawnPower(static_cast<float>(x + 2), static_cast<float>(y + 2));
          b.vy = -b.vy;
          if ((level_ % 3) == 0 && c == COLS - 1) b.vx = -abs(b.vx) - 5.0f;
          hitBrick = true;
          break;
        }
      }
    }

    if (b.y > height) b.active = false;
  }

  if (bricksLeft_ == 0) {
    level_++;
    buildLevel();
    return;
  }

  if (activeBallCount() == 0) endApp();
}

void Breakout76Game::drawRunning(TFT_eSPI& tft) {
  auto drawScene = [this](auto& canvas) {
    TDisplayUi::clear(canvas);
    const String stat = "L" + String(level_) + "  " + String(score_);
    TDisplayUi::header(canvas, "Breakout '76", TFT_ORANGE, stat.c_str());
    canvas.drawRect(4, TDisplayUi::HEADER_H + 3, width - 8, height - TDisplayUi::HEADER_H - 7, TFT_DARKGREY);
    for (uint8_t r = 0; r < ROWS; r++) {
      for (uint8_t c = 0; c < COLS; c++) {
        if (!bricks_[r][c]) continue;
        const uint16_t color = r == 0 ? TFT_RED : (r == 1 ? TFT_ORANGE : (r == 2 ? TFT_YELLOW : (r == 3 ? TFT_GREEN : TFT_CYAN)));
        canvas.fillRoundRect(BRICK_X + c * BRICK_W, BRICK_Y + r * BRICK_H, BRICK_W - 3, BRICK_H - 2, 2, color);
      }
    }
    for (uint8_t p = 0; p < POWERUPS; p++) {
      if (powers_[p].active) {
        if (powers_[p].type == 0) {
          canvas.fillRoundRect(static_cast<int>(powers_[p].x) - 5, static_cast<int>(powers_[p].y), 11, 7, 2, TFT_GREEN);
        } else {
          canvas.drawCircle(static_cast<int>(powers_[p].x), static_cast<int>(powers_[p].y), 5, TFT_MAGENTA);
          canvas.drawLine(static_cast<int>(powers_[p].x) - 4, static_cast<int>(powers_[p].y), static_cast<int>(powers_[p].x) + 4, static_cast<int>(powers_[p].y), TFT_MAGENTA);
        }
      }
    }
    for (uint8_t i = 0; i < BALLS; i++) {
      if (balls_[i].active) {
        canvas.fillCircle(clampInt(static_cast<int>(balls_[i].x), 6, width - 7), clampInt(static_cast<int>(balls_[i].y), 31, height - 7), 4, TFT_WHITE);
      }
    }
    canvas.fillRoundRect(static_cast<int>(paddleX_), PADDLE_Y, paddleW_, 6, 3, wideTimerMs_ > 0 ? TFT_GREEN : TFT_WHITE);
  };

  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), drawScene);
}

void Breakout76Game::drawStart(TFT_eSPI& tft) {
  TDisplayUi::clear(tft);
  loadBestScore();
  if (showStartPromptPage()) {
    TDisplayUi::header(tft, appTitle(), TFT_ORANGE);
    TDisplayUi::centered(tft, "Press", 50, 3, TFT_WHITE);
    TDisplayUi::centered(tft, "to Start", 78, 2, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "B1 start");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);

    TDisplayUi::header(tft, "Top Bricks", TFT_YELLOW);
    TDisplayUi::largeValue(tft, bestScore_ == 0 ? String("--") : String(bestScore_), 54, TFT_YELLOW);
    TDisplayUi::centered(tft, bestScore_ == 0 ? String("") : String(initials), 96, 2, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "Best total bricks");
  } else {
    TDisplayUi::header(tft, appTitle(), TFT_ORANGE);
    for (uint8_t r = 0; r < 4; r++) {
      for (uint8_t c = 0; c < 6; c++) {
        const uint16_t color = r == 0 ? TFT_RED : (r == 1 ? TFT_YELLOW : (r == 2 ? TFT_GREEN : TFT_CYAN));
        tft.fillRoundRect(42 + c * 24, 48 + r * 11, 20, 8, 2, color);
      }
    }
    tft.fillRoundRect(84, 101, 70, 7, 3, TFT_WHITE);
    tft.fillCircle(174, 72, 5, TFT_WHITE);
    tft.drawLine(154, 101, 174, 72, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "B1 right  B2 left");
  }
}

void Breakout76Game::drawEnd(TFT_eSPI& tft) {
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Ball Lost", TFT_RED);
  TDisplayUi::labelValue(tft, 48, "Bricks", String(score_), TFT_ORANGE);
  String best = String(bestScore_);
  if (bestScore_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    best += " ";
    best += initials;
  }
  TDisplayUi::labelValue(tft, 78, "Best", best, TFT_YELLOW);
  TDisplayUi::footer(tft, "B1 retry  Hold menu");
}

int Breakout76Game::clampInt(int value, int minValue, int maxValue) const {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

void Breakout76Game::loadBestScore() {
  if (bestLoaded_) return;
  breakout76Prefs.begin("breakout76", true);
  bestScore_ = breakout76Prefs.getUShort("best", 0);
  bestInitials_ = breakout76Prefs.getUShort("init", PlayerProfile::defaultInitials());
  breakout76Prefs.end();
  bestLoaded_ = true;
}

void Breakout76Game::saveBestScore() {
  bestInitials_ = PlayerProfile::loadInitials();
  breakout76Prefs.begin("breakout76", false);
  breakout76Prefs.putUShort("best", bestScore_);
  breakout76Prefs.putUShort("init", bestInitials_);
  breakout76Prefs.end();
}
