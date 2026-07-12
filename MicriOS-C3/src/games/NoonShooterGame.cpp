#include "NoonShooterGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
Preferences shooterPrefs;
}

NoonShooterGame::NoonShooterGame(uint32_t width, uint32_t height, uint32_t left)
    : App("Noon Shooter", width, height), left_(left) {}

bool NoonShooterGame::hasCustomOverlay() const {
  return true;
}

void NoonShooterGame::onAppReset() {
  loadScores();
  level_ = 1;
  tooEarly_ = false;
  playerLost_ = false;
  startRound();
}

void NoonShooterGame::startRound() {
  duelState_ = DuelState::Waiting;
  waitMs_ = static_cast<uint16_t>(random(1000, 3200));
  elapsedMs_ = 0;
  reactionMs_ = 0;
  animMs_ = 0;
}

void NoonShooterGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  elapsedMs_ += static_cast<uint16_t>(min<uint32_t>(deltaMs, 200));

  if (duelState_ == DuelState::PlayerShot || duelState_ == DuelState::EnemyShot) {
    animMs_ += deltaMs;
    if (animMs_ >= 1050) {
      if (duelState_ == DuelState::PlayerShot) {
        level_++;
        if (level_ > bestLevel_) {
          bestLevel_ = level_;
          saveBestLevel();
        }
        startRound();
      } else {
        endApp();
      }
    }
    return;
  }

  if (duelState_ == DuelState::Waiting) {
    if (input.pressed) {
      tooEarly_ = true;
      playerLost_ = true;
      duelState_ = DuelState::EnemyShot;
      animMs_ = 0;
      return;
    }
    if (elapsedMs_ >= waitMs_) {
      duelState_ = DuelState::Draw;
      elapsedMs_ = waitMs_;
    }
    return;
  }

  if (input.pressed) {
    reactionMs_ = elapsedMs_ - waitMs_;
    if (reactionMs_ < enemyDelayMs() && (bestReactionMs_ == 0 || reactionMs_ < bestReactionMs_)) {
      bestReactionMs_ = reactionMs_;
      saveBestReaction();
    }
    if (reactionMs_ < enemyDelayMs()) {
      duelState_ = DuelState::PlayerShot;
    } else {
      playerLost_ = true;
      duelState_ = DuelState::EnemyShot;
    }
    animMs_ = 0;
  } else if ((elapsedMs_ - waitMs_) >= enemyDelayMs()) {
    reactionMs_ = enemyDelayMs();
    playerLost_ = true;
    duelState_ = DuelState::EnemyShot;
    animMs_ = 0;
  }
}

void NoonShooterGame::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  if (duelState_ == DuelState::PlayerShot || duelState_ == DuelState::EnemyShot) {
    const uint8_t frame = min<uint8_t>(4, animMs_ / 160);
    if (frame < 3) {
      drawRevolver(u8g2, duelState_ == DuelState::PlayerShot, frame);
      return;
    }
  }

  u8g2.drawLine(left_ + 1, 31, left_ + width - 2, 31);
  if (duelState_ == DuelState::EnemyShot && animMs_ >= 480) {
    drawFallenGunslinger(u8g2, left_ + 12, 31, true);
  } else {
    drawGunslinger(u8g2, left_ + 12, 28, true);
  }
  if (duelState_ == DuelState::PlayerShot && animMs_ >= 480) {
    drawFallenGunslinger(u8g2, left_ + 56, 31, false);
  } else {
    drawGunslinger(u8g2, left_ + 56, 28, false);
  }
  u8g2.setFont(u8g2_font_5x8_tr);
  if (duelState_ == DuelState::Waiting) {
    u8g2.drawStr(left_ + 23, 12, "WAIT");
  } else if (duelState_ == DuelState::Draw) {
    u8g2.drawStr(left_ + 22, 12, "DRAW!");
  } else {
    u8g2.drawStr(left_ + 24, 12, duelState_ == DuelState::PlayerShot ? "HIT" : "SHOT");
  }
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 2, 38);
  u8g2.print("L");
  u8g2.print(level_);
  u8g2.print(" enemy ");
  u8g2.print(enemyDelayMs());
}

void NoonShooterGame::drawStart(U8G2& u8g2) {
  loadScores();
  u8g2.drawFrame(0, 0, width + 2, height);
  if (showStartPromptPage()) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(20, 16, "Press");
    u8g2.drawStr(13, 29, "to Start");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 10, "Top Duel");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(3, 23);
    if (bestLevel_ == 0) {
      u8g2.print("--");
    } else {
      u8g2.print(initials);
      u8g2.print(" L");
      u8g2.print(bestLevel_);
      u8g2.setCursor(3, 32);
      if (bestReactionMs_ == 0) {
        u8g2.print("--");
      } else {
        u8g2.print(bestReactionMs_);
        u8g2.print("ms");
      }
    }
  } else {
    u8g2.drawCircle(56, 8, 5);
    u8g2.drawLine(4, 31, 67, 31);
    drawGunslinger(u8g2, 18, 28, true);
    drawGunslinger(u8g2, 46, 28, false);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 10, appTitle());
  }
}

void NoonShooterGame::drawEnd(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, tooEarly_ ? "Too Early" : "You Lost");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(3, 20);
  u8g2.print("React ");
  if (tooEarly_ || reactionMs_ == 0) {
    u8g2.print("--");
  } else {
    u8g2.print(reactionMs_);
    u8g2.print("ms");
  }
  u8g2.setCursor(3, 29);
  u8g2.print("Best L");
  u8g2.print(bestLevel_);
  u8g2.print(" ");
  if (bestLevel_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.print(initials);
    u8g2.print(" ");
  }
  if (bestReactionMs_ == 0) {
    u8g2.print("--");
  } else {
    u8g2.print(bestReactionMs_);
    u8g2.print("ms");
  }
  u8g2.drawStr(3, 38, "Tap retry Hold menu");
}

void NoonShooterGame::loadScores() {
  if (bestLoaded_) {
    return;
  }
  shooterPrefs.begin("shooter", true);
  bestReactionMs_ = shooterPrefs.getUShort("best", 0);
  bestLevel_ = shooterPrefs.getUChar("level", 0);
  bestInitials_ = shooterPrefs.getUShort("init", PlayerProfile::defaultInitials());
  shooterPrefs.end();
  bestLoaded_ = true;
}

void NoonShooterGame::saveBestReaction() {
  bestInitials_ = PlayerProfile::loadInitials();
  shooterPrefs.begin("shooter", false);
  shooterPrefs.putUShort("best", bestReactionMs_);
  shooterPrefs.putUShort("init", bestInitials_);
  shooterPrefs.end();
}

void NoonShooterGame::saveBestLevel() {
  bestInitials_ = PlayerProfile::loadInitials();
  shooterPrefs.begin("shooter", false);
  shooterPrefs.putUChar("level", bestLevel_);
  shooterPrefs.putUShort("init", bestInitials_);
  shooterPrefs.end();
}

uint16_t NoonShooterGame::enemyDelayMs() const {
  if (level_ <= 6) {
    return 900 - (level_ * 100);
  }
  return max<uint16_t>(230, 300 - ((level_ - 6) * 12));
}

void NoonShooterGame::drawRevolver(U8G2& u8g2, bool playerShot, uint8_t frame) {
  const int cx = left_ + 36;
  const int cy = 20;
  u8g2.drawCircle(cx, cy, 10);
  u8g2.drawCircle(cx, cy, 4);
  for (uint8_t i = 0; i < 6; i++) {
    const float angle = static_cast<float>(i) * 1.047f;
    u8g2.drawPixel(cx + static_cast<int>(cosf(angle) * 7), cy + static_cast<int>(sinf(angle) * 7));
  }
  u8g2.drawCircle(cx, cy, 2);
  if (frame >= 1) {
    const int flashX = playerShot ? left_ + 52 : left_ + 20;
    u8g2.drawLine(cx, cy, flashX, cy);
    u8g2.drawLine(flashX, cy - 3, flashX + (playerShot ? 5 : -5), cy);
    u8g2.drawLine(flashX, cy + 3, flashX + (playerShot ? 5 : -5), cy);
  }
  if (frame >= 2) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(left_ + 28, 38, "BANG");
  }
}

void NoonShooterGame::drawGunslinger(U8G2& u8g2, int x, int y, bool facingRight) {
  u8g2.drawCircle(x, y - 12, 2);
  u8g2.drawLine(x, y - 9, x, y - 3);
  u8g2.drawLine(x, y - 3, x - 3, y);
  u8g2.drawLine(x, y - 3, x + 3, y);
  if (facingRight) {
    u8g2.drawLine(x, y - 8, x + 6, y - 8);
    u8g2.drawPixel(x + 7, y - 8);
  } else {
    u8g2.drawLine(x, y - 8, x - 6, y - 8);
    u8g2.drawPixel(x - 7, y - 8);
  }
}

void NoonShooterGame::drawFallenGunslinger(U8G2& u8g2, int x, int y, bool facingRight) {
  const int dir = facingRight ? 1 : -1;
  u8g2.drawCircle(x, y - 3, 2);
  u8g2.drawLine(x + dir * 2, y - 3, x + dir * 11, y - 2);
  u8g2.drawLine(x + dir * 5, y - 2, x + dir * 2, y);
  u8g2.drawLine(x + dir * 7, y - 2, x + dir * 11, y);
}
