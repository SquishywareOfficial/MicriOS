#include "Breakout76Game.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../CydFramebuffer.h"
#include "../../CydUi.h"

namespace {
Preferences breakout76Prefs;
constexpr uint8_t BRICK_W = 31;
constexpr uint8_t BRICK_H = 13;
constexpr uint8_t BRICK_X = 21;
constexpr uint8_t BRICK_Y = 42;
constexpr uint8_t PADDLE_Y = 192;
constexpr uint8_t NORMAL_PADDLE_W = 58;
constexpr uint8_t WIDE_PADDLE_W = 88;
constexpr float BALL_RADIUS = 4.0f;
constexpr float PLAY_LEFT = 5.0f;
constexpr float PLAY_RIGHT = 314.0f;
constexpr float PLAY_TOP = static_cast<float>(CydUi::HEADER_H + 4);

float clampFloat(float value, float minValue, float maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}
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
  paddleX_ = static_cast<float>((width - NORMAL_PADDLE_W) / 2);
  paddleVel_ = 0.0f;
  paddleDir_ = 1;
  paddleW_ = NORMAL_PADDLE_W;
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
  balls_[slot].x = 35.0f + static_cast<float>(random(0, width - 70));
  balls_[slot].y = 137.0f + static_cast<float>(random(0, 13));
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
    paddleW_ = WIDE_PADDLE_W;
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
    if (wideTimerMs_ == 0) paddleW_ = NORMAL_PADDLE_W;
  }

  const float oldPaddle = paddleX_;
  paddleX_ += static_cast<float>(inputDir) * 172.0f * dt;
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
    const float previousX = b.x;
    const float previousY = b.y;
    b.x += b.vx * dt;
    b.y += b.vy * dt;

    const float minBallX = PLAY_LEFT + BALL_RADIUS;
    const float maxBallX = PLAY_RIGHT - BALL_RADIUS;
    const float minBallY = PLAY_TOP + BALL_RADIUS;
    if (b.x < minBallX) {
      b.x = minBallX;
      if (b.vx < 0.0f) b.vx = -b.vx;
    } else if (b.x > maxBallX) {
      b.x = maxBallX;
      if (b.vx > 0.0f) b.vx = -b.vx;
    }
    if (b.y < minBallY) {
      b.y = minBallY;
      if (b.vy < 0.0f) b.vy = -b.vy;
    }

    if (b.vy > 0.0f && previousY + BALL_RADIUS <= PADDLE_Y &&
        b.y + BALL_RADIUS >= PADDLE_Y &&
        b.x + BALL_RADIUS >= paddleX_ &&
        b.x - BALL_RADIUS <= paddleX_ + paddleW_) {
      const float hit = ((b.x - paddleX_) / static_cast<float>(paddleW_)) - 0.5f;
      b.vx = hit * 115.0f + paddleVel_ * 0.22f;
      b.vy = -(70.0f + static_cast<float>(level_) * 4.0f);
      b.y = PADDLE_Y - BALL_RADIUS;
    }

    bool hitBrick = false;
    for (uint8_t r = 0; r < ROWS && !hitBrick; r++) {
      for (uint8_t c = 0; c < COLS; c++) {
        if (!bricks_[r][c]) continue;
        const float brickLeft = static_cast<float>(BRICK_X + c * BRICK_W);
        const float brickTop = static_cast<float>(BRICK_Y + r * BRICK_H);
        const float brickRight = brickLeft + BRICK_W - 3.0f;
        const float brickBottom = brickTop + BRICK_H - 2.0f;
        const float closestX = clampFloat(b.x, brickLeft, brickRight);
        const float closestY = clampFloat(b.y, brickTop, brickBottom);
        const float dx = b.x - closestX;
        const float dy = b.y - closestY;
        if (dx * dx + dy * dy <= BALL_RADIUS * BALL_RADIUS) {
          bricks_[r][c] = false;
          bricksLeft_--;
          score_++;
          if (score_ > bestScore_) {
            bestScore_ = score_;
            saveBestScore();
          }
          spawnPower(brickLeft + 2.0f, brickTop + 2.0f);

          // Resolve against the face crossed this frame. This prevents the
          // ball remaining embedded and bouncing repeatedly along a wall.
          if (previousY + BALL_RADIUS <= brickTop && b.vy > 0.0f) {
            b.y = brickTop - BALL_RADIUS;
            b.vy = -abs(b.vy);
          } else if (previousY - BALL_RADIUS >= brickBottom && b.vy < 0.0f) {
            b.y = brickBottom + BALL_RADIUS;
            b.vy = abs(b.vy);
          } else if (previousX + BALL_RADIUS <= brickLeft && b.vx > 0.0f) {
            b.x = brickLeft - BALL_RADIUS;
            b.vx = -abs(b.vx);
          } else if (previousX - BALL_RADIUS >= brickRight && b.vx < 0.0f) {
            b.x = brickRight + BALL_RADIUS;
            b.vx = abs(b.vx);
          } else {
            // Corner/high-delta fallback: reflect on the shallower overlap.
            const float overlapX = BALL_RADIUS - abs(dx);
            const float overlapY = BALL_RADIUS - abs(dy);
            if (overlapX < overlapY) b.vx = -b.vx;
            else b.vy = -b.vy;
          }
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
    CydUi::clear(canvas);
    const String stat = "L" + String(level_) + "  " + String(score_);
    CydUi::header(canvas, "Breakout '76", TFT_ORANGE, stat.c_str());
    canvas.drawRect(4, CydUi::HEADER_H + 3, width - 8, height - CydUi::HEADER_H - 7, TFT_DARKGREY);
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
        canvas.fillCircle(static_cast<int>(balls_[i].x),
                          static_cast<int>(balls_[i].y),
                          static_cast<int>(BALL_RADIUS), TFT_WHITE);
      }
    }
    canvas.fillRoundRect(static_cast<int>(paddleX_), PADDLE_Y, paddleW_, 6, 3, wideTimerMs_ > 0 ? TFT_GREEN : TFT_WHITE);
  };

  CydFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), drawScene);
}

void Breakout76Game::drawStart(TFT_eSPI& tft) {
  CydUi::clear(tft);
  loadBestScore();
  if (showStartPromptPage()) {
    CydUi::header(tft, appTitle(), TFT_ORANGE);
    CydUi::centered(tft, "Press", 50, 3, TFT_WHITE);
    CydUi::centered(tft, "to Start", 78, 2, TFT_LIGHTGREY);
    CydUi::footer(tft, "Tap Start");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);

    CydUi::header(tft, "Top Bricks", TFT_YELLOW);
    CydUi::largeValue(tft, bestScore_ == 0 ? String("--") : String(bestScore_), 54, TFT_YELLOW);
    CydUi::centered(tft, bestScore_ == 0 ? String("") : String(initials), 96, 2, TFT_LIGHTGREY);
    CydUi::footer(tft, "Best total bricks");
  } else {
    CydUi::header(tft, appTitle(), TFT_ORANGE);
    for (uint8_t r = 0; r < 4; r++) {
      for (uint8_t c = 0; c < 6; c++) {
        const uint16_t color = r == 0 ? TFT_RED : (r == 1 ? TFT_YELLOW : (r == 2 ? TFT_GREEN : TFT_CYAN));
        tft.fillRoundRect(42 + c * 24, 48 + r * 11, 20, 8, 2, color);
      }
    }
    tft.fillRoundRect(84, 101, 70, 7, 3, TFT_WHITE);
    tft.fillCircle(174, 72, 5, TFT_WHITE);
    tft.drawLine(154, 101, 174, 72, TFT_LIGHTGREY);
    CydUi::footer(tft, "Hold either side to move");
  }
}

void Breakout76Game::drawEnd(TFT_eSPI& tft) {
  CydUi::clear(tft);
  CydUi::header(tft, "Ball Lost", TFT_RED);
  CydUi::labelValue(tft, 48, "Bricks", String(score_), TFT_ORANGE);
  String best = String(bestScore_);
  if (bestScore_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    best += " ";
    best += initials;
  }
  CydUi::labelValue(tft, 78, "Best", best, TFT_YELLOW);
  CydUi::footer(tft, "Tap Play Again");
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
