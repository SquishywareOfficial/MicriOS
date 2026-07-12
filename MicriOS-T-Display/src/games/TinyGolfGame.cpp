#include "TinyGolfGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

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
constexpr int COURSE_X = 10;
constexpr int COURSE_Y = 31;
constexpr int COURSE_W = 220;
constexpr int COURSE_H = 85;
constexpr int LOGICAL_W = 70;
constexpr int LOGICAL_H = 40;

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

int sx(float x) {
  return COURSE_X + static_cast<int>((x / static_cast<float>(LOGICAL_W - 1)) * static_cast<float>(COURSE_W) + 0.5f);
}

int sy(float y) {
  return COURSE_Y + static_cast<int>((y / static_cast<float>(LOGICAL_H - 1)) * static_cast<float>(COURSE_H) + 0.5f);
}

int sw(float w) {
  return max(1, static_cast<int>((w / static_cast<float>(LOGICAL_W)) * static_cast<float>(COURSE_W) + 0.5f));
}

int sh(float h) {
  return max(1, static_cast<int>((h / static_cast<float>(LOGICAL_H)) * static_cast<float>(COURSE_H) + 0.5f));
}
}

TinyGolfGame::TinyGolfGame(uint32_t width, uint32_t height)
    : App("Tiny Golf", width, height) {}

bool TinyGolfGame::hasCustomOverlay() const {
  return true;
}

void TinyGolfGame::onAppReset() {
  loadBestScore();
  totalStrokes_ = 0;
  holeIndex_ = 0;
  loadHole(holeIndex_);
}

void TinyGolfGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)b2;
  switch (playState_) {
    case PlayState::Aim:
      updateAim(deltaMs);
      if (b1.click) {
        playState_ = PlayState::Power;
        power_ = 0.25f;
        powerDir_ = 1;
      }
      break;
    case PlayState::Power:
      updatePower(deltaMs);
      if (b1.click) {
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

void TinyGolfGame::drawRunning(TFT_eSPI& tft) {
  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    const String stat = "H" + String(holeIndex_ + 1) + " S" + String(totalStrokes_);
    TDisplayUi::header(canvas, "Tiny Golf", TFT_GREEN, stat.c_str());
    drawCourse(canvas);
    if (playState_ == PlayState::Aim) {
      drawAim(canvas);
      TDisplayUi::footer(canvas, "B1 set angle");
    } else if (playState_ == PlayState::Power) {
      drawAim(canvas);
      drawPower(canvas);
      TDisplayUi::footer(canvas, "B1 set power");
    } else if (playState_ == PlayState::Holed) {
      TDisplayUi::centered(canvas, "Nice", 68, 3, TFT_YELLOW);
      TDisplayUi::footer(canvas, "Holed");
    } else {
      TDisplayUi::footer(canvas, "Rolling");
    }
  });
}

void TinyGolfGame::drawStart(TFT_eSPI& tft) {
  TDisplayUi::clear(tft);
  loadBestScore();
  if (showStartPromptPage()) {
    TDisplayUi::header(tft, appTitle(), TFT_GREEN);
    TDisplayUi::centered(tft, "Press", 50, 3, TFT_WHITE);
    TDisplayUi::centered(tft, "to Start", 78, 2, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "B1 start");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);

    TDisplayUi::header(tft, "Top Score", TFT_YELLOW);
    TDisplayUi::largeValue(tft, bestScore_ == 0 ? String("--") : String(bestScore_), 54, TFT_YELLOW);
    TDisplayUi::centered(tft, bestScore_ == 0 ? String("") : String(initials), 96, 2, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "Lowest score wins");
  } else {
    TDisplayUi::header(tft, appTitle(), TFT_GREEN);
    tft.drawRoundRect(35, 47, 168, 56, 6, TFT_DARKGREEN);
    tft.fillCircle(60, 84, 5, TFT_WHITE);
    tft.drawCircle(175, 65, 8, TFT_DARKGREY);
    tft.fillCircle(175, 65, 3, TFT_BLACK);
    tft.fillRect(92, 48, 8, 41, TFT_DARKGREEN);
    tft.fillRect(124, 61, 55, 7, TFT_DARKGREEN);
    tft.drawLine(62, 82, 120, 62, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "Low score wins");
  }
}

void TinyGolfGame::drawEnd(TFT_eSPI& tft) {
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Course Done", TFT_GREEN);
  TDisplayUi::labelValue(tft, 48, "Score", String(totalStrokes_), TFT_GREEN);
  String best = "--";
  if (bestScore_ == 0) {
    best = "--";
  } else {
    best = String(bestScore_);
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    best += " ";
    best += initials;
  }
  TDisplayUi::labelValue(tft, 78, "Best", best, TFT_YELLOW);
  TDisplayUi::footer(tft, "B1 retry  Hold menu");
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

template <typename Canvas>
void TinyGolfGame::drawCourse(Canvas& tft) {
  const CourseHole& hole = HOLES[holeIndex_];
  tft.drawRoundRect(COURSE_X, COURSE_Y, COURSE_W, COURSE_H, 4, TFT_DARKGREEN);
  tft.drawFastHLine(COURSE_X + 1, sy(HUD_H), COURSE_W - 2, TFT_DARKGREEN);
  tft.drawCircle(sx(hole.cupX), sy(hole.cupY), 7, TFT_LIGHTGREY);
  tft.fillCircle(sx(hole.cupX), sy(hole.cupY), 4, TFT_BLACK);
  for (uint8_t i = 0; i < COURSE_WALL_COUNT; i++) {
    const CourseWall& wall = hole.walls[i];
    if (wall.w <= 0 || wall.h <= 0) {
      continue;
    }
    tft.fillRoundRect(sx(wall.x), sy(wall.y), sw(wall.w), sh(wall.h), 2, TFT_LIGHTGREY);
  }
  for (uint8_t i = 0; i < COURSE_GUIDE_COUNT; i++) {
    const CourseGuide& guide = hole.guides[i];
    if (guide.x1 == 0 && guide.y1 == 0 && guide.x2 == 0 && guide.y2 == 0) {
      continue;
    }
    tft.drawLine(sx(guide.x1), sy(guide.y1), sx(guide.x2), sy(guide.y2), TFT_YELLOW);
    tft.drawLine(sx(guide.x1) + 1, sy(guide.y1), sx(guide.x2) + 1, sy(guide.y2), TFT_ORANGE);
  }
  tft.fillCircle(sx(ballX_), sy(ballY_), 5, TFT_WHITE);
  tft.drawCircle(sx(ballX_), sy(ballY_), 5, TFT_DARKGREY);
}

template <typename Canvas>
void TinyGolfGame::drawHud(Canvas& tft) {

  tft.setCursor(2, 6);
  tft.print("H");
  tft.print(holeIndex_ + 1);
  tft.print(" S");
  tft.print(totalStrokes_);
}

template <typename Canvas>
void TinyGolfGame::drawAim(Canvas& tft) {
  const int aimLen = 8;
  const float x2 = ballX_ + cosf(aimAngle_) * aimLen;
  const float y2 = ballY_ + sinf(aimAngle_) * aimLen;
  tft.drawLine(sx(ballX_), sy(ballY_), sx(x2), sy(y2), TFT_CYAN);
  tft.fillCircle(sx(x2), sy(y2), 2, TFT_CYAN);
}

template <typename Canvas>
void TinyGolfGame::drawPower(Canvas& tft) {
  TDisplayUi::bar(tft, 154, 8, 72, 10, power_, power_ > 0.78f ? TFT_RED : TFT_YELLOW);
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
  const float maxX = static_cast<float>(LOGICAL_W - 3);
  const float minY = static_cast<float>(HUD_H + 2);
  const float maxY = static_cast<float>(LOGICAL_H - 3);
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
