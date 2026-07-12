#include "KnifeThrowGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
Preferences knifePrefs;
constexpr float TWO_PI_F = 6.2831853f;
}

KnifeThrowGame::KnifeThrowGame(uint32_t width, uint32_t height, uint32_t left)
    : App("Knife Throw", width, height), left_(left) {}

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

void KnifeThrowGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
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

  if (!throwActive_ && input.click) {
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

void KnifeThrowGame::drawPerson(U8G2& u8g2, int cx, int cy) const {
  const int hx = cx + static_cast<int>(rotateX(0.0f, -9.0f));
  const int hy = cy + static_cast<int>(rotateY(0.0f, -9.0f));
  const int sx = cx + static_cast<int>(rotateX(0.0f, -6.0f));
  const int sy = cy + static_cast<int>(rotateY(0.0f, -6.0f));
  const int px = cx + static_cast<int>(rotateX(0.0f, 7.0f));
  const int py = cy + static_cast<int>(rotateY(0.0f, 7.0f));
  const int la = cx + static_cast<int>(rotateX(-5.0f, -1.0f));
  const int lay = cy + static_cast<int>(rotateY(-5.0f, -1.0f));
  const int ra = cx + static_cast<int>(rotateX(5.0f, -1.0f));
  const int ray = cy + static_cast<int>(rotateY(5.0f, -1.0f));
  const int ll = cx + static_cast<int>(rotateX(-4.0f, 10.0f));
  const int lly = cy + static_cast<int>(rotateY(-4.0f, 10.0f));
  const int rl = cx + static_cast<int>(rotateX(4.0f, 10.0f));
  const int rly = cy + static_cast<int>(rotateY(4.0f, 10.0f));
  u8g2.drawCircle(hx, hy, 2);
  u8g2.drawLine(sx, sy, px, py);
  u8g2.drawLine(sx, sy, la, lay);
  u8g2.drawLine(sx, sy, ra, ray);
  u8g2.drawLine(px, py, ll, lly);
  u8g2.drawLine(px, py, rl, rly);
}

void KnifeThrowGame::drawReticle(U8G2& u8g2, int cx, int cy) const {
  const int x = cx + static_cast<int>(cosf(reticleAngle_) * reticleRadius_);
  const int y = cy + static_cast<int>(sinf(reticleAngle_) * reticleRadius_);
  u8g2.drawLine(x - 3, y, x + 3, y);
  u8g2.drawLine(x, y - 3, x, y + 3);
}

void KnifeThrowGame::drawBoard(U8G2& u8g2, int cx, int cy) {
  u8g2.drawCircle(cx, cy, 14);
  u8g2.drawCircle(cx, cy, 10);
  u8g2.drawLine(cx + static_cast<int>(cosf(boardAngle_) * 14), cy + static_cast<int>(sinf(boardAngle_) * 14),
                cx - static_cast<int>(cosf(boardAngle_) * 14), cy - static_cast<int>(sinf(boardAngle_) * 14));
  drawPerson(u8g2, cx, cy);
  for (uint8_t i = 0; i < knifeCount_; i++) {
    const int x = cx + static_cast<int>(rotateX(knives_[i].lx, knives_[i].ly));
    const int y = cy + static_cast<int>(rotateY(knives_[i].lx, knives_[i].ly));
    u8g2.drawLine(x - 2, y, x + 2, y);
    u8g2.drawLine(x, y - 2, x, y + 2);
  }
  if (!throwActive_) {
    drawReticle(u8g2, cx, cy);
  } else {
    const float progress = static_cast<float>(throwMs_) / 850.0f;
    const int sx = 2;
    const int sy = 35;
    const int tx = cx + static_cast<int>(thrownScreenX_);
    const int ty = cy + static_cast<int>(thrownScreenY_);
    const int x = sx + static_cast<int>((tx - sx) * progress);
    const int y = sy + static_cast<int>((ty - sy) * progress);
    u8g2.drawLine(x - 4, y + 2, x, y);
  }
}

void KnifeThrowGame::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  drawBoard(u8g2, left_ + 42, 20);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 2, 8);
  u8g2.print(score_);
  u8g2.drawStr(left_ + 2, 38, throwActive_ ? "Knife..." : "Tap");
}

void KnifeThrowGame::drawStart(U8G2& u8g2) {
  loadBest();
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
    if (bestScore_ == 0) u8g2.print("--");
    else {
      u8g2.print(initials);
      u8g2.print(" ");
      u8g2.print(bestScore_);
    }
  } else {
    boardAngle_ += 0.0f;
    drawBoard(u8g2, 52, 22);
    u8g2.drawLine(9, 31, 30, 18);
    u8g2.drawLine(30, 18, 35, 18);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 10, appTitle());
  }
}

void KnifeThrowGame::drawEnd(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, hitPerson_ ? "Hit Person" : "Hit Knife");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(3, 21);
  u8g2.print("Score ");
  u8g2.print(score_);
  u8g2.setCursor(3, 30);
  u8g2.print("Best ");
  u8g2.print(bestScore_);
  u8g2.drawStr(3, 38, "Tap retry Hold menu");
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
