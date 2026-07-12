#include "TinyGolfGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
constexpr uint8_t COURSE_HOLE_COUNT = 5;
constexpr uint8_t COURSE_WALL_COUNT = 4;
constexpr uint8_t COURSE_GUIDE_COUNT = 3;
constexpr float FULL_CIRCLE = 6.2831853f;
constexpr float AIM_SPEED = 1.9f;
constexpr float POWER_SPEED = 1.4f;
constexpr float MIN_SHOT_SPEED = 16.0f;
constexpr float MAX_SHOT_SPEED = 86.0f;
constexpr float FRICTION = 24.0f;
constexpr float STOP_SPEED = 4.0f;
constexpr float CUP_CAPTURE_RADIUS = 3.6f;
constexpr float CUP_CAPTURE_SPEED = 34.0f;

Preferences golfScorePrefs;

struct CourseWall {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

struct CourseGuide {
  int x1 = 0;
  int y1 = 0;
  int x2 = 0;
  int y2 = 0;
};

struct CourseHole {
  int startX = 0;
  int startY = 0;
  int cupX = 0;
  int cupY = 0;
  CourseWall walls[COURSE_WALL_COUNT] = {};
  CourseGuide guides[COURSE_GUIDE_COUNT] = {};
};

// Tiny Golf maps use screen-space coordinates inside the 70x40 game area.
// X increases left-to-right. Y increases top-to-bottom. The top 7 pixels are
// reserved for the HUD, so playable objects should usually stay at y >= 8.
//
// Each hole is:
//   {ballStartX, ballStartY, cupX, cupY, walls, diagonalGuides}
//
// Walls are filled rectangles:
//   {x, y, width, height}
// A wall at {17, 10, 3, 24} starts at pixel (17,10), is 3 pixels wide,
// and extends down 24 pixels.
//
// Diagonal guides are bounce lines:
//   {x1, y1, x2, y2}
// These are drawn as thin 45-ish degree bumpers. The ball reflects across
// the line, so flipping the endpoints' slope changes the bounce direction:
//   {2, 27, 8, 37}  slopes down-right like "\"
//   {27, 37, 35, 29} slopes up-right like "/"
//
// Use {0, 0, 0, 0} for unused walls or guides.
const CourseHole HOLES[COURSE_HOLE_COUNT] = {
    {8, 32, 59, 16, {{27, 12, 4, 20}, {43, 8, 4, 18}, {0, 0, 0, 0}, {0, 0, 0, 0}}, {}},
    {10, 15, 58, 31, {{20, 22, 32, 3}, {36, 10, 3, 12}, {0, 0, 0, 0}, {0, 0, 0, 0}}, {}},
    {9, 11, 61, 11, {{21, 10, 3, 18}, {34, 17, 3, 21}, {51, 8, 3, 20}, {0, 0, 0, 0}}, {{2, 27, 8, 37}, {27, 37, 35, 29}, {44, 7, 52, 16}}},
    {12, 13, 58, 33, {{22, 8, 4, 17}, {22, 29, 29, 3}, {48, 13, 4, 16}, {0, 0, 0, 0}}, {}},
    {9, 31, 61, 20, {{27, 12, 15, 3}, {27, 25, 15, 3}, {39, 15, 3, 10}, {0, 0, 0, 0}}, {}},
};

float clampFloat(float value, float minValue, float maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

float distanceToSegmentSq(float px, float py, float x1, float y1, float x2, float y2) {
  const float vx = x2 - x1;
  const float vy = y2 - y1;
  const float wx = px - x1;
  const float wy = py - y1;
  const float lenSq = vx * vx + vy * vy;
  if (lenSq <= 0.0f) {
    const float dx = px - x1;
    const float dy = py - y1;
    return dx * dx + dy * dy;
  }
  const float t = clampFloat((wx * vx + wy * vy) / lenSq, 0.0f, 1.0f);
  const float nx = x1 + t * vx;
  const float ny = y1 + t * vy;
  const float dx = px - nx;
  const float dy = py - ny;
  return dx * dx + dy * dy;
}
}

TinyGolfGame::TinyGolfGame(uint32_t width, uint32_t height, uint32_t left)
    : App("Tiny Golf", width, height), left_(left) {}

bool TinyGolfGame::hasCustomOverlay() const {
  return true;
}

void TinyGolfGame::onAppReset() {
  loadBestScore();
  totalStrokes_ = 0;
  holeIndex_ = 0;
  loadHole(holeIndex_);
}

void TinyGolfGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  switch (playState_) {
    case PlayState::Aim:
      updateAim(deltaMs);
      if (input.click) {
        playState_ = PlayState::Power;
        power_ = 0.25f;
        powerDir_ = 1;
      }
      break;
    case PlayState::Power:
      updatePower(deltaMs);
      if (input.click) {
        startShot();
      }
      break;
    case PlayState::Rolling:
      updateBall(deltaMs);
      break;
    case PlayState::Holed:
      holedTimerMs_ += deltaMs;
      if (holedTimerMs_ >= 900) {
        finishHole();
      }
      break;
  }
}

void TinyGolfGame::drawRunning(U8G2& u8g2) {
  drawCourse(u8g2);
  drawHud(u8g2);
  if (playState_ == PlayState::Aim) {
    drawAim(u8g2);
  } else if (playState_ == PlayState::Power) {
    drawAim(u8g2);
    drawPower(u8g2);
  } else if (playState_ == PlayState::Holed) {
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(left_ + 20, 24, "Nice");
  }
}

void TinyGolfGame::drawStart(U8G2& u8g2) {
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
    u8g2.drawStr(3, 10, "Top Score");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(3, 24);
    if (bestScore_ == 0) {
      u8g2.print("--");
    } else {
      u8g2.print(initials);
      u8g2.print(" ");
      u8g2.print(bestScore_);
    }
  } else {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 10, appTitle());
    u8g2.drawStr(3, 34, "Low score wins");
  }
}

void TinyGolfGame::drawEnd(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, "Course Done");
  u8g2.setCursor(3, 19);
  u8g2.print("Score:");
  u8g2.print(totalStrokes_);
  u8g2.setCursor(3, 29);
  u8g2.print("Best:");
  if (bestScore_ == 0) {
    u8g2.print("--");
  } else {
    u8g2.print(bestScore_);
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.print(" ");
    u8g2.print(initials);
  }
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 38, "Tap retry Hold menu");
}

void TinyGolfGame::loadBestScore() {
  if (bestScoreLoaded_) {
    return;
  }
  golfScorePrefs.begin("golf", true);
  bestScore_ = golfScorePrefs.getUChar("best", 0);
  bestInitials_ = golfScorePrefs.getUShort("init", PlayerProfile::defaultInitials());
  golfScorePrefs.end();
  bestScoreLoaded_ = true;
}

void TinyGolfGame::saveBestScore() {
  bestInitials_ = PlayerProfile::loadInitials();
  golfScorePrefs.begin("golf", false);
  golfScorePrefs.putUChar("best", bestScore_);
  golfScorePrefs.putUShort("init", bestInitials_);
  golfScorePrefs.end();
}

void TinyGolfGame::loadHole(uint8_t holeIndex) {
  const CourseHole& hole = HOLES[holeIndex];
  ballX_ = static_cast<float>(hole.startX);
  ballY_ = static_cast<float>(hole.startY);
  ballVx_ = 0.0f;
  ballVy_ = 0.0f;
  holeStrokes_ = 0;
  aimAngle_ = 0.0f;
  power_ = 0.25f;
  powerDir_ = 1;
  holedTimerMs_ = 0;
  playState_ = PlayState::Aim;
}

void TinyGolfGame::updateAim(uint32_t deltaMs) {
  const float speedScale = aimLineNearCup() ? 0.32f : 1.0f;
  aimAngle_ += AIM_SPEED * speedScale * static_cast<float>(deltaMs) * 0.001f;
  if (aimAngle_ >= FULL_CIRCLE) {
    aimAngle_ -= FULL_CIRCLE;
  }
}

void TinyGolfGame::updatePower(uint32_t deltaMs) {
  power_ += static_cast<float>(powerDir_) * POWER_SPEED * static_cast<float>(deltaMs) * 0.001f;
  if (power_ >= 1.0f) {
    power_ = 1.0f;
    powerDir_ = -1;
  } else if (power_ <= 0.2f) {
    power_ = 0.2f;
    powerDir_ = 1;
  }
}

void TinyGolfGame::updateBall(uint32_t deltaMs) {
  const float deltaSec = static_cast<float>(deltaMs) * 0.001f;
  const float previousX = ballX_;
  const float previousY = ballY_;
  ballX_ += ballVx_ * deltaSec;
  ballY_ += ballVy_ * deltaSec;

  bounceBounds();
  bounceWalls(previousX, previousY);
  bounceGuides(previousX, previousY);

  float speed = sqrtf(ballVx_ * ballVx_ + ballVy_ * ballVy_);
  if (speed > 0.0f) {
    if (hitsCup() && speed <= CUP_CAPTURE_SPEED) {
      ballVx_ = 0.0f;
      ballVy_ = 0.0f;
      playState_ = PlayState::Holed;
      holedTimerMs_ = 0;
      return;
    }
    speed = max(0.0f, speed - FRICTION * deltaSec);
    if (speed <= STOP_SPEED) {
      ballVx_ = 0.0f;
      ballVy_ = 0.0f;
      if (hitsCup()) {
        playState_ = PlayState::Holed;
        holedTimerMs_ = 0;
      } else {
        playState_ = PlayState::Aim;
      }
    } else {
      const float angle = atan2f(ballVy_, ballVx_);
      ballVx_ = cosf(angle) * speed;
      ballVy_ = sinf(angle) * speed;
      if (hitsCup() && speed <= CUP_CAPTURE_SPEED) {
        ballVx_ = 0.0f;
        ballVy_ = 0.0f;
        playState_ = PlayState::Holed;
        holedTimerMs_ = 0;
      }
    }
  }
}

void TinyGolfGame::drawCourse(U8G2& u8g2) {
  const CourseHole& hole = HOLES[holeIndex_];
  u8g2.drawFrame(left_, 0, width, height);
  u8g2.drawLine(left_ + 1, HUD_H, left_ + width - 2, HUD_H);
  u8g2.drawCircle(left_ + hole.cupX, hole.cupY, 2);
  u8g2.drawPixel(left_ + hole.cupX, hole.cupY);
  for (uint8_t i = 0; i < COURSE_WALL_COUNT; i++) {
    const CourseWall& wall = hole.walls[i];
    if (wall.w <= 0 || wall.h <= 0) {
      continue;
    }
    u8g2.drawBox(left_ + wall.x, wall.y, wall.w, wall.h);
  }
  for (uint8_t i = 0; i < COURSE_GUIDE_COUNT; i++) {
    const CourseGuide& guide = hole.guides[i];
    if (guide.x1 == 0 && guide.y1 == 0 && guide.x2 == 0 && guide.y2 == 0) {
      continue;
    }
    u8g2.drawLine(left_ + guide.x1, guide.y1, left_ + guide.x2, guide.y2);
  }
  u8g2.drawDisc(left_ + static_cast<int>(ballX_ + 0.5f), static_cast<int>(ballY_ + 0.5f), 1);
}

void TinyGolfGame::drawHud(U8G2& u8g2) {
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 2, 6);
  u8g2.print("H");
  u8g2.print(holeIndex_ + 1);
  u8g2.print(" S");
  u8g2.print(totalStrokes_);
}

void TinyGolfGame::drawAim(U8G2& u8g2) {
  const int aimLen = 8;
  const int x2 = static_cast<int>(ballX_ + cosf(aimAngle_) * aimLen + 0.5f);
  const int y2 = static_cast<int>(ballY_ + sinf(aimAngle_) * aimLen + 0.5f);
  u8g2.drawLine(left_ + static_cast<int>(ballX_), static_cast<int>(ballY_), left_ + x2, y2);
}

void TinyGolfGame::drawPower(U8G2& u8g2) {
  const int barX = left_ + 45;
  const int barW = 20;
  u8g2.drawFrame(barX, 2, barW, 4);
  u8g2.drawBox(barX + 1, 3, static_cast<int>((barW - 2) * power_), 2);
}

void TinyGolfGame::startShot() {
  const float shotSpeed = MIN_SHOT_SPEED + (MAX_SHOT_SPEED - MIN_SHOT_SPEED) * power_;
  ballVx_ = cosf(aimAngle_) * shotSpeed;
  ballVy_ = sinf(aimAngle_) * shotSpeed;
  totalStrokes_++;
  holeStrokes_++;
  playState_ = PlayState::Rolling;
}

void TinyGolfGame::finishHole() {
  if ((holeIndex_ + 1) >= HOLE_COUNT) {
    finishCourse();
    return;
  }
  holeIndex_++;
  loadHole(holeIndex_);
}

void TinyGolfGame::finishCourse() {
  if (bestScore_ == 0 || totalStrokes_ < bestScore_) {
    bestScore_ = totalStrokes_;
    saveBestScore();
  }
  endApp();
}

bool TinyGolfGame::aimLineNearCup() const {
  const CourseHole& hole = HOLES[holeIndex_];
  const float aimLen = 11.0f;
  const float x2 = ballX_ + cosf(aimAngle_) * aimLen;
  const float y2 = ballY_ + sinf(aimAngle_) * aimLen;
  return distanceToSegmentSq(
      static_cast<float>(hole.cupX),
      static_cast<float>(hole.cupY),
      ballX_,
      ballY_,
      x2,
      y2) <= 12.0f;
}

bool TinyGolfGame::hitsCup() const {
  const CourseHole& hole = HOLES[holeIndex_];
  const float dx = ballX_ - static_cast<float>(hole.cupX);
  const float dy = ballY_ - static_cast<float>(hole.cupY);
  return (dx * dx + dy * dy) <= (CUP_CAPTURE_RADIUS * CUP_CAPTURE_RADIUS);
}

void TinyGolfGame::bounceBounds() {
  const float minX = 2.0f;
  const float maxX = static_cast<float>(width - 3);
  const float minY = static_cast<float>(HUD_H + 2);
  const float maxY = static_cast<float>(height - 3);
  if (ballX_ < minX || ballX_ > maxX) {
    ballX_ = clampFloat(ballX_, minX, maxX);
    ballVx_ = -ballVx_ * 0.88f;
  }
  if (ballY_ < minY || ballY_ > maxY) {
    ballY_ = clampFloat(ballY_, minY, maxY);
    ballVy_ = -ballVy_ * 0.88f;
  }
}

void TinyGolfGame::bounceWalls(float previousX, float previousY) {
  const CourseHole& hole = HOLES[holeIndex_];
  for (uint8_t i = 0; i < COURSE_WALL_COUNT; i++) {
    const CourseWall& wall = hole.walls[i];
    if (wall.w <= 0 || wall.h <= 0) {
      continue;
    }
    const float nearestX = clampFloat(ballX_, static_cast<float>(wall.x), static_cast<float>(wall.x + wall.w));
    const float nearestY = clampFloat(ballY_, static_cast<float>(wall.y), static_cast<float>(wall.y + wall.h));
    const float dx = ballX_ - nearestX;
    const float dy = ballY_ - nearestY;
    if ((dx * dx + dy * dy) > (BALL_R * BALL_R)) {
      continue;
    }

    const bool wasLeftOrRight = previousX < wall.x || previousX > (wall.x + wall.w);
    const bool wasAboveOrBelow = previousY < wall.y || previousY > (wall.y + wall.h);
    if (wasLeftOrRight && !wasAboveOrBelow) {
      ballX_ = previousX;
      ballVx_ = -ballVx_ * 0.88f;
    } else if (wasAboveOrBelow) {
      ballY_ = previousY;
      ballVy_ = -ballVy_ * 0.88f;
    } else {
      ballX_ = previousX;
      ballY_ = previousY;
      ballVx_ = -ballVx_ * 0.88f;
      ballVy_ = -ballVy_ * 0.88f;
    }
  }
}

void TinyGolfGame::bounceGuides(float previousX, float previousY) {
  const CourseHole& hole = HOLES[holeIndex_];
  for (uint8_t i = 0; i < COURSE_GUIDE_COUNT; i++) {
    const CourseGuide& guide = hole.guides[i];
    if (guide.x1 == 0 && guide.y1 == 0 && guide.x2 == 0 && guide.y2 == 0) {
      continue;
    }
    if (distanceToSegmentSq(
            ballX_,
            ballY_,
            static_cast<float>(guide.x1),
            static_cast<float>(guide.y1),
            static_cast<float>(guide.x2),
            static_cast<float>(guide.y2)) > ((BALL_R + 1.0f) * (BALL_R + 1.0f))) {
      continue;
    }

    const float sx = static_cast<float>(guide.x2 - guide.x1);
    const float sy = static_cast<float>(guide.y2 - guide.y1);
    const float len = sqrtf(sx * sx + sy * sy);
    if (len <= 0.0f) {
      continue;
    }

    const float nx = -sy / len;
    const float ny = sx / len;
    const float dot = ballVx_ * nx + ballVy_ * ny;
    ballX_ = previousX;
    ballY_ = previousY;
    ballVx_ = (ballVx_ - 2.0f * dot * nx) * 0.88f;
    ballVy_ = (ballVy_ - 2.0f * dot * ny) * 0.88f;
  }
}
