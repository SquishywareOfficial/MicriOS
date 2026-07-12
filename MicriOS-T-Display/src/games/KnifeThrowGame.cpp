#include "KnifeThrowGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
Preferences knifePrefs;
constexpr float TWO_PI_F = 6.2831853f;
constexpr float BOARD_SCALE = 2.35f;
constexpr int BOARD_CX = 150;
constexpr int BOARD_CY = 73;
}

KnifeThrowGame::KnifeThrowGame(uint32_t width, uint32_t height)
    : App("Knife Throw", width, height) {}

bool KnifeThrowGame::hasCustomOverlay() const {
  return true;
}

void KnifeThrowGame::onAppReset() {
  loadBest();
  boardAngle_ = random(0, 628) * 0.01f;
  boardSpeed_ = 1.25f;
  reticleAngle_ = random(0, 628) * 0.01f;
  reticleRadius_ = 7.0f;
  reticleSpeed_ = 2.1f;
  throwMs_ = 0;
  throwActive_ = false;
  hitPerson_ = false;
  knifeCount_ = 0;
  score_ = 0;
}

void KnifeThrowGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)b2;
  const float dt = static_cast<float>(deltaMs) * 0.001f;
  boardAngle_ += boardSpeed_ * dt;
  if (boardAngle_ >= TWO_PI_F) {
    boardAngle_ -= TWO_PI_F;
  }

  if (!throwActive_) {
    reticleAngle_ += reticleSpeed_ * dt;
    if (reticleAngle_ >= TWO_PI_F) reticleAngle_ -= TWO_PI_F;
    if (random(0, 100) < 4) {
      reticleRadius_ = static_cast<float>(random(3, 12));
      reticleSpeed_ = static_cast<float>(random(15, 34)) * 0.1f;
      if (random(0, 2) == 0) reticleSpeed_ = -reticleSpeed_;
    }
  }

  if (!throwActive_ && b1.click) {
    throwKnife();
  }

  if (throwActive_) {
    throwMs_ += deltaMs;
    if (throwMs_ >= 850) {
      resolveThrow();
    }
  }
}

void KnifeThrowGame::throwKnife() {
  thrownScreenX_ = cosf(reticleAngle_) * reticleRadius_;
  thrownScreenY_ = sinf(reticleAngle_) * reticleRadius_;
  throwMs_ = 0;
  throwActive_ = true;
}

void KnifeThrowGame::resolveThrow() {
  throwActive_ = false;
  const float c = cosf(-boardAngle_);
  const float s = sinf(-boardAngle_);
  const float lx = thrownScreenX_ * c - thrownScreenY_ * s;
  const float ly = thrownScreenX_ * s + thrownScreenY_ * c;

  if (localPointHitsPerson(lx, ly)) {
    hitPerson_ = true;
    endApp();
    return;
  }

  for (uint8_t i = 0; i < knifeCount_; i++) {
    const float dx = lx - knives_[i].lx;
    const float dy = ly - knives_[i].ly;
    if ((dx * dx + dy * dy) < 10.0f) {
      hitPerson_ = false;
      endApp();
      return;
    }
  }

  if (knifeCount_ < 16) {
    knives_[knifeCount_].lx = lx;
    knives_[knifeCount_].ly = ly;
    knifeCount_++;
  } else {
    knifeCount_ = 0;
  }
  score_++;
  if (score_ > bestScore_) {
    bestScore_ = score_;
    saveBest();
  }
  boardSpeed_ = min(4.6f, boardSpeed_ + 0.09f);
  reticleSpeed_ += reticleSpeed_ > 0.0f ? 0.08f : -0.08f;
}

bool KnifeThrowGame::localPointHitsPerson(float lx, float ly) const {
  const bool body = abs(static_cast<int>(lx)) <= 2 && ly >= -7.0f && ly <= 7.0f;
  const bool head = (lx * lx + (ly + 9.0f) * (ly + 9.0f)) <= 9.0f;
  return body || head;
}

float KnifeThrowGame::rotateX(float lx, float ly) const {
  return lx * cosf(boardAngle_) - ly * sinf(boardAngle_);
}

float KnifeThrowGame::rotateY(float lx, float ly) const {
  return lx * sinf(boardAngle_) + ly * cosf(boardAngle_);
}

template <typename Canvas>
void KnifeThrowGame::drawPerson(Canvas& tft, int cx, int cy) const {
  const int hx = cx + static_cast<int>(rotateX(0.0f, -9.0f) * BOARD_SCALE);
  const int hy = cy + static_cast<int>(rotateY(0.0f, -9.0f) * BOARD_SCALE);
  const int sx = cx + static_cast<int>(rotateX(0.0f, -6.0f) * BOARD_SCALE);
  const int sy = cy + static_cast<int>(rotateY(0.0f, -6.0f) * BOARD_SCALE);
  const int px = cx + static_cast<int>(rotateX(0.0f, 7.0f) * BOARD_SCALE);
  const int py = cy + static_cast<int>(rotateY(0.0f, 7.0f) * BOARD_SCALE);
  const int la = cx + static_cast<int>(rotateX(-5.0f, -1.0f) * BOARD_SCALE);
  const int lay = cy + static_cast<int>(rotateY(-5.0f, -1.0f) * BOARD_SCALE);
  const int ra = cx + static_cast<int>(rotateX(5.0f, -1.0f) * BOARD_SCALE);
  const int ray = cy + static_cast<int>(rotateY(5.0f, -1.0f) * BOARD_SCALE);
  const int ll = cx + static_cast<int>(rotateX(-4.0f, 10.0f) * BOARD_SCALE);
  const int lly = cy + static_cast<int>(rotateY(-4.0f, 10.0f) * BOARD_SCALE);
  const int rl = cx + static_cast<int>(rotateX(4.0f, 10.0f) * BOARD_SCALE);
  const int rly = cy + static_cast<int>(rotateY(4.0f, 10.0f) * BOARD_SCALE);
  tft.drawCircle(hx, hy, 5, TFT_WHITE);
  tft.drawLine(sx, sy, px, py, TFT_WHITE);
  tft.drawLine(sx, sy, la, lay, TFT_WHITE);
  tft.drawLine(sx, sy, ra, ray, TFT_WHITE);
  tft.drawLine(px, py, ll, lly, TFT_WHITE);
  tft.drawLine(px, py, rl, rly, TFT_WHITE);
}

template <typename Canvas>
void KnifeThrowGame::drawReticle(Canvas& tft, int cx, int cy) const {
  const int x = cx + static_cast<int>(cosf(reticleAngle_) * reticleRadius_ * BOARD_SCALE);
  const int y = cy + static_cast<int>(sinf(reticleAngle_) * reticleRadius_ * BOARD_SCALE);
  tft.drawCircle(x, y, 5, TFT_YELLOW);
  tft.drawLine(x - 8, y, x + 8, y, TFT_YELLOW);
  tft.drawLine(x, y - 8, x, y + 8, TFT_YELLOW);
}

template <typename Canvas>
void KnifeThrowGame::drawBoard(Canvas& tft, int cx, int cy) {
  tft.fillCircle(cx, cy, static_cast<int>(15 * BOARD_SCALE), TFT_MAROON);
  tft.drawCircle(cx, cy, static_cast<int>(15 * BOARD_SCALE), TFT_ORANGE);
  tft.drawCircle(cx, cy, static_cast<int>(10 * BOARD_SCALE), TFT_YELLOW);
  tft.drawLine(cx + static_cast<int>(cosf(boardAngle_) * 15 * BOARD_SCALE), cy + static_cast<int>(sinf(boardAngle_) * 15 * BOARD_SCALE),
               cx - static_cast<int>(cosf(boardAngle_) * 15 * BOARD_SCALE), cy - static_cast<int>(sinf(boardAngle_) * 15 * BOARD_SCALE), TFT_YELLOW);
  drawPerson(tft, cx, cy);
  for (uint8_t i = 0; i < knifeCount_; i++) {
    const int x = cx + static_cast<int>(rotateX(knives_[i].lx, knives_[i].ly) * BOARD_SCALE);
    const int y = cy + static_cast<int>(rotateY(knives_[i].lx, knives_[i].ly) * BOARD_SCALE);
    tft.drawLine(x - 5, y, x + 5, y, TFT_LIGHTGREY);
    tft.drawLine(x, y - 5, x, y + 5, TFT_WHITE);
  }
  if (!throwActive_) {
    drawReticle(tft, cx, cy);
  } else {
    const float progress = static_cast<float>(throwMs_) / 850.0f;
    const int sx = 20;
    const int sy = 112;
    const int tx = cx + static_cast<int>(thrownScreenX_ * BOARD_SCALE);
    const int ty = cy + static_cast<int>(thrownScreenY_ * BOARD_SCALE);
    const int x = sx + static_cast<int>((tx - sx) * progress);
    const int y = sy + static_cast<int>((ty - sy) * progress);
    tft.drawLine(x - 10, y + 5, x, y, TFT_WHITE);
    tft.drawLine(x - 5, y + 5, x - 10, y + 5, TFT_LIGHTGREY);
  }
}

void KnifeThrowGame::drawRunning(TFT_eSPI& tft) {
  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    TDisplayUi::header(canvas, "Knife Throw", TFT_ORANGE, (String("S") + String(score_)).c_str());
    drawBoard(canvas, BOARD_CX, BOARD_CY);
    TDisplayUi::footer(canvas, throwActive_ ? "Knife in flight..." : "B1 throw");
  });
}

void KnifeThrowGame::drawStart(TFT_eSPI& tft) { tft.fillScreen(TFT_BLACK);
  loadBest();
  TDisplayUi::clear(tft);
  if (showStartPromptPage()) {
    TDisplayUi::header(tft, "Knife Throw", TFT_ORANGE);
    TDisplayUi::centered(tft, "Press to Start", 56, 2, TFT_WHITE);
    TDisplayUi::footer(tft, "Do not hit the person");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    TDisplayUi::header(tft, "Top Score", TFT_ORANGE);
    String score = bestScore_ == 0 ? "--" : String(initials) + " " + String(bestScore_);
    TDisplayUi::largeValue(tft, score, 54, TFT_ORANGE);
    TDisplayUi::footer(tft, "Best clean throws");
  } else {
    TDisplayUi::header(tft, "Knife Throw", TFT_ORANGE);
    drawBoard(tft, BOARD_CX, BOARD_CY);
    tft.drawLine(22, 112, 72, 88, TFT_WHITE);
    tft.drawLine(72, 88, 84, 90, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "Time the spinning board");
  }
}

void KnifeThrowGame::drawEnd(TFT_eSPI& tft) { tft.fillScreen(TFT_BLACK);
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, hitPerson_ ? "Hit Person" : "Hit Knife", TFT_RED);
  TDisplayUi::labelValue(tft, 49, "Score", String(score_), TFT_YELLOW);
  String best = String(bestScore_);
  if (bestScore_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    best += " ";
    best += initials;
  }
  TDisplayUi::labelValue(tft, 78, "Best", best, TFT_GREEN);
  TDisplayUi::footer(tft, "B1 retry / B1 hold menu");
}

void KnifeThrowGame::loadBest() {
  if (bestLoaded_) return;
  knifePrefs.begin("knife", true);
  bestScore_ = knifePrefs.getUShort("best", 0);
  bestInitials_ = knifePrefs.getUShort("init", PlayerProfile::defaultInitials());
  knifePrefs.end();
  bestLoaded_ = true;
}

void KnifeThrowGame::saveBest() {
  bestInitials_ = PlayerProfile::loadInitials();
  knifePrefs.begin("knife", false);
  knifePrefs.putUShort("best", bestScore_);
  knifePrefs.putUShort("init", bestInitials_);
  knifePrefs.end();
}
