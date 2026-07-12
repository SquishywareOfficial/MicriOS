#include "NoonShooterGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
Preferences shooterPrefs;

template <typename Canvas>
void drawRevolverShape(Canvas& canvas, bool playerShot, uint8_t frame) {
  const int cx = 120;
  const int cy = 70;
  canvas.drawCircle(cx, cy, 36, TFT_LIGHTGREY);
  canvas.drawCircle(cx, cy, 14, TFT_LIGHTGREY);
  for (uint8_t i = 0; i < 6; i++) {
    const float angle = static_cast<float>(i) * 1.047f;
    canvas.fillCircle(cx + static_cast<int>(cosf(angle) * 25), cy + static_cast<int>(sinf(angle) * 25), 3, TFT_DARKGREY);
  }
  canvas.fillCircle(cx, cy, 6, TFT_BLACK);
  if (frame >= 1) {
    const int flashX = playerShot ? 188 : 52;
    canvas.drawLine(cx, cy, flashX, cy, TFT_YELLOW);
    canvas.drawLine(flashX, cy - 12, flashX + (playerShot ? 22 : -22), cy, TFT_ORANGE);
    canvas.drawLine(flashX, cy + 12, flashX + (playerShot ? 22 : -22), cy, TFT_ORANGE);
  }
  if (frame >= 2) {
    TDisplayUi::centered(canvas, "BANG", 105, 3, TFT_YELLOW);
  }
}

template <typename Canvas>
void drawGunslingerShape(Canvas& canvas, int x, int y, bool facingRight) {
  canvas.drawCircle(x, y - 36, 6, TFT_WHITE);
  canvas.drawLine(x, y - 29, x, y - 10, TFT_WHITE);
  canvas.drawLine(x, y - 10, x - 10, y, TFT_WHITE);
  canvas.drawLine(x, y - 10, x + 10, y, TFT_WHITE);
  if (facingRight) {
    canvas.drawLine(x, y - 25, x + 22, y - 25, TFT_WHITE);
    canvas.drawFastHLine(x + 22, y - 26, 8, TFT_LIGHTGREY);
  } else {
    canvas.drawLine(x, y - 25, x - 22, y - 25, TFT_WHITE);
    canvas.drawFastHLine(x - 30, y - 26, 8, TFT_LIGHTGREY);
  }
}

template <typename Canvas>
void drawFallenGunslingerShape(Canvas& canvas, int x, int y, bool facingRight) {
  const int dir = facingRight ? 1 : -1;
  canvas.drawCircle(x, y - 8, 6, TFT_WHITE);
  canvas.drawLine(x + dir * 7, y - 8, x + dir * 34, y - 4, TFT_WHITE);
  canvas.drawLine(x + dir * 18, y - 4, x + dir * 8, y, TFT_WHITE);
  canvas.drawLine(x + dir * 24, y - 4, x + dir * 36, y, TFT_WHITE);
}
}

NoonShooterGame::NoonShooterGame(uint32_t width, uint32_t height)
    : App("Noon Shooter", width, height) {}

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

void NoonShooterGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)b2;
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
    if (b1.pressed) {
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

  if (b1.pressed) {
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

void NoonShooterGame::drawRunning(TFT_eSPI& tft) {
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    TDisplayUi::clear(canvas);
    TDisplayUi::header(canvas, "Noon Shooter", TFT_ORANGE, (String("L") + String(level_)).c_str());
    if (duelState_ == DuelState::PlayerShot || duelState_ == DuelState::EnemyShot) {
      const uint8_t frame = min<uint8_t>(4, animMs_ / 160);
      if (frame < 3) {
        drawRevolverShape(canvas, duelState_ == DuelState::PlayerShot, frame);
        return;
      }
    }

    canvas.fillCircle(202, 43, 12, TFT_YELLOW);
    canvas.drawLine(0, 105, width, 105, TFT_ORANGE);
    if (duelState_ == DuelState::EnemyShot && animMs_ >= 480) {
      drawFallenGunslingerShape(canvas, 62, 104, true);
    } else {
      drawGunslingerShape(canvas, 62, 101, true);
    }
    if (duelState_ == DuelState::PlayerShot && animMs_ >= 480) {
      drawFallenGunslingerShape(canvas, 178, 104, false);
    } else {
      drawGunslingerShape(canvas, 178, 101, false);
    }

    if (duelState_ == DuelState::Waiting) {
      TDisplayUi::centered(canvas, "WAIT", 42, 4, TFT_YELLOW);
      TDisplayUi::footer(canvas, "Do not draw early");
    } else if (duelState_ == DuelState::Draw) {
      TDisplayUi::centered(canvas, "DRAW!", 42, 4, TFT_RED);
      TDisplayUi::footer(canvas, "B1 fire");
    } else {
      TDisplayUi::centered(canvas, duelState_ == DuelState::PlayerShot ? "HIT" : "SHOT", 42, 4, duelState_ == DuelState::PlayerShot ? TFT_GREEN : TFT_RED);
      TDisplayUi::footer(canvas, (String("Enemy ") + String(enemyDelayMs()) + "ms").c_str());
    }
  });
}

void NoonShooterGame::drawStart(TFT_eSPI& tft) { tft.fillScreen(TFT_BLACK);
  loadScores();
  TDisplayUi::clear(tft);
  if (showStartPromptPage()) {
    TDisplayUi::header(tft, "Noon Shooter", TFT_ORANGE);
    TDisplayUi::centered(tft, "Press to Start", 56, 2, TFT_WHITE);
    TDisplayUi::footer(tft, "Wait for DRAW!");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    TDisplayUi::header(tft, "Top Duel", TFT_ORANGE);
    String level = bestLevel_ == 0 ? "--" : String(initials) + " L" + String(bestLevel_);
    String react = bestReactionMs_ == 0 ? "--" : String(bestReactionMs_) + "ms";
    TDisplayUi::labelValue(tft, 52, "Level", level, TFT_YELLOW);
    TDisplayUi::labelValue(tft, 80, "React", react, TFT_GREEN);
    TDisplayUi::footer(tft, "Best level and reaction");
  } else {
    TDisplayUi::header(tft, "Noon Shooter", TFT_ORANGE);
    tft.fillCircle(202, 43, 12, TFT_YELLOW);
    tft.drawLine(0, 105, width, 105, TFT_ORANGE);
    drawGunslinger(tft, 68, 101, true);
    drawGunslinger(tft, 172, 101, false);
    TDisplayUi::footer(tft, "Western reaction duel");
  }
}

void NoonShooterGame::drawEnd(TFT_eSPI& tft) { tft.fillScreen(TFT_BLACK);
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, tooEarly_ ? "Too Early" : "You Lost", TFT_RED);
  String react = "--";
  if (tooEarly_ || reactionMs_ == 0) {
    react = "--";
  } else {
    react = String(reactionMs_) + "ms";
  }
  String best = "L" + String(bestLevel_) + " ";
  if (bestLevel_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    best += initials;
    best += " ";
  }
  if (bestReactionMs_ == 0) {
    best += "--";
  } else {
    best += String(bestReactionMs_) + "ms";
  }
  TDisplayUi::labelValue(tft, 49, "React", react, TFT_YELLOW);
  TDisplayUi::labelValue(tft, 78, "Best", best, TFT_GREEN);
  TDisplayUi::footer(tft, "B1 retry / B1 hold menu");
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

void NoonShooterGame::drawRevolver(TFT_eSPI& tft, bool playerShot, uint8_t frame) {
  drawRevolverShape(tft, playerShot, frame);
}

void NoonShooterGame::drawGunslinger(TFT_eSPI& tft, int x, int y, bool facingRight) {
  drawGunslingerShape(tft, x, y, facingRight);
}

void NoonShooterGame::drawFallenGunslinger(TFT_eSPI& tft, int x, int y, bool facingRight) {
  drawFallenGunslingerShape(tft, x, y, facingRight);
}
