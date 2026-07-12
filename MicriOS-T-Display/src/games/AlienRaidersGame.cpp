#include "AlienRaidersGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
namespace {
Preferences alienPrefs;
constexpr uint8_t PORTRAIT_ROTATION = 0;
constexpr int PORTRAIT_W = 135;
constexpr int PORTRAIT_H = 240;
constexpr int HUD_H = 27;
constexpr int SHIP_Y = 196;
constexpr int LANE_LEFT = 30;
constexpr int LANE_STEP = 38;
constexpr int PLAY_TOP = 31;
constexpr int PLAY_BOTTOM = 226;
constexpr float BOSS_SPEED = 7.0f;
constexpr float BOSS_TOP_Y = PLAY_TOP + 8.0f;
constexpr float BOSS_BOTTOM_Y = SHIP_Y - 52.0f;

void setPortrait(TFT_eSPI& tft) {
  tft.setRotation(PORTRAIT_ROTATION);
}

void clearPortrait(TFT_eSPI& tft) {
  setPortrait(tft);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
}

template <typename Canvas>
void drawPortraitHeader(Canvas& canvas, const char* title, uint16_t color, const String& stat = String()) {
  canvas.fillRect(0, 0, PORTRAIT_W, HUD_H, TFT_BLACK);
  canvas.setTextSize(1);
  canvas.setTextColor(color, TFT_BLACK);
  canvas.drawString(title, 5, 6);
  if (stat.length() > 0) {
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString(stat, PORTRAIT_W - canvas.textWidth(stat) - 5, 6);
  }
  canvas.drawFastHLine(0, HUD_H - 1, PORTRAIT_W, TFT_DARKGREY);
}

template <typename Canvas>
void drawPortraitFooter(Canvas& canvas, const char* text) {
  canvas.fillRect(0, PORTRAIT_H - 17, PORTRAIT_W, 17, TFT_BLACK);
  canvas.drawFastHLine(0, PORTRAIT_H - 18, PORTRAIT_W, TFT_DARKGREY);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(text, 5, PORTRAIT_H - 13);
}

template <typename Canvas>
void drawCentered(Canvas& canvas, const String& text, int y, uint8_t size, uint16_t color) {
  canvas.setTextSize(size);
  canvas.setTextColor(color, TFT_BLACK);
  canvas.drawString(text, (PORTRAIT_W - canvas.textWidth(text)) / 2, y);
}

template <typename Canvas, typename EnemyLike>
void drawEnemyShape(Canvas& canvas, const EnemyLike& enemy, int laneX0, int laneX1, int laneX2, int laneXEnemy) {
  const int y = static_cast<int>(enemy.x);
  if (enemy.boss) {
    const int left = laneX0 - 18;
    const int right = laneX2 + 18;
    canvas.fillRect(left + 8, y + 6, right - left - 16, 20, TFT_PURPLE);
    canvas.drawTriangle(laneX1, y + 42, left, y, right, y, TFT_MAGENTA);
    if (enemy.hp > 0) canvas.drawRoundRect(left - 3, y - 4, right - left + 6, 51, 8, TFT_BLUE);
    if (enemy.hp > enemy.maxHp / 3) canvas.drawRoundRect(left - 7, y - 8, right - left + 14, 59, 10, TFT_CYAN);
    if (enemy.hp > (enemy.maxHp * 2) / 3) canvas.drawRoundRect(left - 11, y - 12, right - left + 22, 67, 12, TFT_WHITE);

    canvas.setTextSize(2);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.drawString(String(static_cast<int>(enemy.hp)), laneX1 - 8, y + 16);
    return;
  }

  canvas.fillTriangle(laneXEnemy, y + 13, laneXEnemy - 11, y - 9, laneXEnemy + 11, y - 9, enemy.maxHp > 1 ? TFT_ORANGE : TFT_RED);
  canvas.drawTriangle(laneXEnemy, y + 13, laneXEnemy - 11, y - 9, laneXEnemy + 11, y - 9, TFT_WHITE);
  const uint8_t shieldLayers = enemy.hp > 1 ? enemy.hp - 1 : 0;
  if (shieldLayers >= 1) canvas.drawCircle(laneXEnemy, y, 14, TFT_CYAN);
  if (shieldLayers >= 2) canvas.drawCircle(laneXEnemy, y, 18, TFT_BLUE);
  if (shieldLayers >= 3) canvas.drawCircle(laneXEnemy, y, 22, TFT_LIGHTGREY);
  if (shieldLayers >= 4) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.drawString(String(static_cast<int>(enemy.hp)), laneXEnemy - 4, y - 4);
  }
}

template <typename Canvas, typename PowerLike>
void drawPowerShape(Canvas& canvas, const PowerLike& power, int laneXPower) {
  const int y = static_cast<int>(power.x);
  if (power.type == 5) {
    canvas.fillCircle(laneXPower, y, 8, TFT_DARKGREY);
    canvas.drawCircle(laneXPower, y, 8, TFT_WHITE);
    canvas.drawLine(laneXPower, y - 8, laneXPower + 6, y - 14, TFT_WHITE);
    canvas.fillCircle(laneXPower + 8, y - 14, 2, TFT_ORANGE);
    return;
  }
  canvas.drawCircle(laneXPower, y, 8, TFT_GREEN);
  canvas.drawLine(laneXPower - 6, y, laneXPower + 6, y, TFT_GREEN);
  canvas.drawLine(laneXPower, y - 6, laneXPower, y + 6, TFT_GREEN);
}

template <typename Canvas>
void drawIntroFrame(Canvas& canvas, uint16_t timerMs, int lane0, int lane1, int lane2) {
  drawPortraitHeader(canvas, "Alien Raiders", TFT_CYAN);
  const int stationY = 184;
  const int shipY = 164 - min<int>(92, timerMs / 18);
  const int enemyY = 48;

  canvas.fillRoundRect(36, stationY, 63, 35, 4, TFT_DARKGREY);
  canvas.drawRoundRect(36, stationY, 63, 35, 4, TFT_LIGHTGREY);
  canvas.fillRect(46, stationY + 8, 13, 8, TFT_BLUE);
  canvas.fillRect(76, stationY + 8, 13, 8, TFT_BLUE);
  canvas.fillTriangle(lane1, shipY, lane1 - 10, shipY + 22, lane1 + 10, shipY + 22, TFT_CYAN);
  canvas.drawTriangle(lane1, shipY, lane1 - 10, shipY + 22, lane1 + 10, shipY + 22, TFT_WHITE);
  if (timerMs > 1000) {
    canvas.fillTriangle(lane0, enemyY + 22, lane0 - 11, enemyY, lane0 + 11, enemyY, TFT_RED);
    canvas.fillTriangle(lane2, enemyY + 42, lane2 - 11, enemyY + 20, lane2 + 11, enemyY + 20, TFT_ORANGE);
    drawCentered(canvas, "Defend", 121, 2, TFT_YELLOW);
    drawCentered(canvas, "Station", 143, 2, TFT_YELLOW);
  } else {
    drawCentered(canvas, "Launch", 130, 2, TFT_LIGHTGREY);
  }
}
}

AlienRaidersGame::AlienRaidersGame(uint32_t width, uint32_t height)
    : App("Alien Raiders", width, height) {}

bool AlienRaidersGame::hasCustomOverlay() const {
  return true;
}

void AlienRaidersGame::onAppReset() {
  loadBestScore();
  shipLane_ = 1;
  health_ = 5;
  shieldHits_ = 0;
  rapidMs_ = spreadMs_ = doubleMs_ = 0;
  weaponLevel_ = 0;
  spawnTimerMs_ = 0;
  shotTimerMs_ = 0;
  bossShotTimerMs_ = 0;
  introTimerMs_ = 0;
  nextBossScore_ = 100;
  introActive_ = true;
  bossActive_ = false;
  spreadShotToggle_ = false;
  bossDirection_ = 1;
  score_ = 0;
  for (uint8_t i = 0; i < ENEMIES; i++) enemies_[i].active = false;
  for (uint8_t i = 0; i < SHOTS; i++) shots_[i].active = false;
  for (uint8_t i = 0; i < ENEMY_SHOTS; i++) enemyShots_[i].active = false;
  for (uint8_t i = 0; i < POWERS; i++) powers_[i].active = false;
}

void AlienRaidersGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  const float dt = static_cast<float>(deltaMs) * 0.001f;
  if (introActive_) {
    introTimerMs_ += deltaMs;
    if (introTimerMs_ >= 2100) {
      introActive_ = false;
      spawnTimerMs_ = 0;
      shotTimerMs_ = 0;
      bossShotTimerMs_ = 0;
    }
    return;
  }

  if (b1.click) shipLane_ = (shipLane_ + LANES - 1) % LANES;
  if (b2.click) shipLane_ = (shipLane_ + 1) % LANES;

  if (rapidMs_ > 0) rapidMs_ = deltaMs >= rapidMs_ ? 0 : rapidMs_ - deltaMs;
  if (spreadMs_ > 0) spreadMs_ = deltaMs >= spreadMs_ ? 0 : spreadMs_ - deltaMs;
  if (doubleMs_ > 0) doubleMs_ = deltaMs >= doubleMs_ ? 0 : doubleMs_ - deltaMs;

  if (!bossActive_ && score_ >= nextBossScore_) {
    spawnBoss();
  }

  const uint16_t spawnEvery = max<int>(360, 1040 - (score_ / 7) * 32);
  spawnTimerMs_ += deltaMs;
  if (!bossActive_ && spawnTimerMs_ >= spawnEvery) {
    spawnTimerMs_ = 0;
    spawnEnemy();
  }

  shotTimerMs_ += deltaMs;
  const bool rapid = rapidMs_ > 0 || weaponLevel_ >= 1;
  const bool doubleShot = doubleMs_ > 0 || weaponLevel_ >= 2;
  const bool spread = spreadMs_ > 0 || weaponLevel_ >= 3;
  const uint16_t shotEvery = weaponLevel_ >= 6 ? 210 : weaponLevel_ >= 5 ? 250 : weaponLevel_ >= 4 ? 310 : rapid ? 410 : 610;
  if (shotTimerMs_ >= shotEvery) {
    shotTimerMs_ = 0;
    fireShot(shipLane_, 0);
    if (doubleShot) fireShot(shipLane_, 0);
    if (spread) {
      spreadShotToggle_ = !spreadShotToggle_;
      if (spreadShotToggle_) {
        if (shipLane_ > 0) fireShot(shipLane_, -1);
        if (shipLane_ + 1 < LANES) fireShot(shipLane_, 1);
      }
    }
  }

  if (bossActive_) {
    bossShotTimerMs_ += deltaMs;
    const uint16_t bossShotEvery = max<int>(360, 760 - score_ / 2);
    if (bossShotTimerMs_ >= bossShotEvery) {
      bossShotTimerMs_ = 0;
      fireBossShot(random(-1, 2));
    }
  }

  for (uint8_t i = 0; i < ENEMIES; i++) {
    if (!enemies_[i].active) continue;
    if (enemies_[i].boss) {
      enemies_[i].x += BOSS_SPEED * static_cast<float>(bossDirection_) * dt;
      if (enemies_[i].x >= BOSS_BOTTOM_Y) {
        enemies_[i].x = BOSS_BOTTOM_Y;
        bossDirection_ = -1;
      } else if (enemies_[i].x <= BOSS_TOP_Y) {
        enemies_[i].x = BOSS_TOP_Y;
        bossDirection_ = 1;
      }
      continue;
    }

    const float base = 50.0f + min<uint16_t>(45, score_ / 3);
    enemies_[i].x += (base - (enemies_[i].maxHp - 1) * 5.0f) * dt;
    if (enemies_[i].x > SHIP_Y + 18.0f) {
      enemies_[i].active = false;
      loseHealth();
    }
  }

  for (uint8_t i = 0; i < SHOTS; i++) {
    if (!shots_[i].active) continue;
    shots_[i].y -= 165.0f * dt;
    shots_[i].x += static_cast<float>(shots_[i].dy) * 34.0f * dt;
    if (shots_[i].y < PLAY_TOP || shots_[i].x < 0.0f || shots_[i].x > PORTRAIT_W) shots_[i].active = false;
  }

  for (uint8_t i = 0; i < ENEMY_SHOTS; i++) {
    if (!enemyShots_[i].active) continue;
    enemyShots_[i].y += 96.0f * dt;
    enemyShots_[i].x += static_cast<float>(enemyShots_[i].dy) * 28.0f * dt;
    if (enemyShots_[i].y > SHIP_Y - 14.0f && enemyShots_[i].y < SHIP_Y + 15.0f &&
        abs(static_cast<int>(enemyShots_[i].x) - laneX(shipLane_)) <= 11) {
      enemyShots_[i].active = false;
      loseHealth();
    } else if (enemyShots_[i].y > PLAY_BOTTOM || enemyShots_[i].x < 0.0f || enemyShots_[i].x > PORTRAIT_W) {
      enemyShots_[i].active = false;
    }
  }

  for (uint8_t i = 0; i < POWERS; i++) {
    if (!powers_[i].active) continue;
    powers_[i].x += 50.0f * dt;
    if (powers_[i].x > SHIP_Y - 14.0f && powers_[i].x < SHIP_Y + 15.0f && powers_[i].lane == shipLane_) {
      applyPower(powers_[i].type);
      powers_[i].active = false;
    } else if (powers_[i].x > PLAY_BOTTOM) {
      powers_[i].active = false;
    }
  }

  for (uint8_t e = 0; e < ENEMIES; e++) {
    if (!enemies_[e].active) continue;
    for (uint8_t s = 0; s < SHOTS; s++) {
      if (!shots_[s].active) continue;
      const bool bossHit = enemies_[e].boss && shots_[s].y >= enemies_[e].x && shots_[s].y <= enemies_[e].x + 38.0f &&
                           shots_[s].x >= laneX(0) - 18 && shots_[s].x <= laneX(2) + 18;
      const bool laneHit = !enemies_[e].boss &&
                           abs(static_cast<int>(shots_[s].x) - laneX(enemies_[e].lane)) <= 13 &&
                           shots_[s].y >= enemies_[e].x - 12.0f && shots_[s].y <= enemies_[e].x + 16.0f;
      if (bossHit || laneHit) {
        shots_[s].active = false;
        if (enemies_[e].hp > 0) enemies_[e].hp--;
        if (enemies_[e].hp == 0) {
          const float px = enemies_[e].x;
          const uint8_t lane = enemies_[e].lane;
          const bool boss = enemies_[e].boss;
          enemies_[e].active = false;
          enemies_[e].boss = false;
          score_ += boss ? 20 : 1;
          if (boss) {
            bossActive_ = false;
            nextBossScore_ += 100;
          }
          if (score_ > bestScore_) {
            bestScore_ = score_;
            saveBestScore();
          }
          if (!boss) spawnPower(px, lane);
          else if (weaponLevel_ < 6) weaponLevel_++;
        }
        break;
      }
    }
  }
}

void AlienRaidersGame::spawnEnemy() {
  for (uint8_t i = 0; i < ENEMIES; i++) {
    if (!enemies_[i].active) {
      const uint8_t roll = random(0, 100);
      uint8_t hp = 1;
      if (score_ > 20 && roll > 62) hp = 2;
      if (score_ > 70 && roll > 68) hp = 3;
      if (score_ > 140 && roll > 72) hp = 4;
      if (score_ > 230 && roll > 76) hp = 5;
      if (score_ > 340 && roll > 80) hp = 6;
      if (score_ > 500 && roll > 84) hp = 7;
      enemies_[i].active = true;
      enemies_[i].boss = false;
      enemies_[i].x = static_cast<float>(PLAY_TOP + 4);
      enemies_[i].lane = random(0, LANES);
      enemies_[i].hp = hp;
      enemies_[i].maxHp = hp;
      return;
    }
  }
}

void AlienRaidersGame::spawnBoss() {
  for (uint8_t i = 0; i < ENEMIES; i++) {
    enemies_[i].active = false;
    enemies_[i].boss = false;
  }
  for (uint8_t i = 0; i < POWERS; i++) powers_[i].active = false;
  enemies_[0].active = true;
  enemies_[0].boss = true;
  enemies_[0].x = static_cast<float>(PLAY_TOP + 8);
  enemies_[0].lane = 1;
  enemies_[0].hp = 28 + min<uint16_t>(120, score_ / 4);
  enemies_[0].maxHp = enemies_[0].hp;
  bossActive_ = true;
  bossDirection_ = 1;
  bossShotTimerMs_ = 250;
}

void AlienRaidersGame::fireShot(uint8_t lane, int8_t dy) {
  for (uint8_t i = 0; i < SHOTS; i++) {
    if (!shots_[i].active) {
      shots_[i].active = true;
      shots_[i].x = static_cast<float>(laneX(lane) + (doubleMs_ > 0 ? static_cast<int>((i % 2) * 4 - 2) : 0));
      shots_[i].lane = lane;
      shots_[i].y = static_cast<float>(SHIP_Y - 16);
      shots_[i].dy = dy;
      return;
    }
  }
}

void AlienRaidersGame::spawnPower(float x, uint8_t lane) {
  if (random(0, 100) > 7) return;
  for (uint8_t i = 0; i < POWERS; i++) {
    if (!powers_[i].active) {
      powers_[i].active = true;
      powers_[i].x = x;
      powers_[i].lane = lane;
      powers_[i].type = random(0, 6);
      return;
    }
  }
}

void AlienRaidersGame::applyPower(uint8_t type) {
  if (type == 0) shieldHits_ = min<uint8_t>(8, shieldHits_ + 2);
  else if (type == 1 && health_ < 9) health_++;
  else if (type >= 2 && type <= 4 && weaponLevel_ < 6) weaponLevel_++;
  else smartBomb();
}

void AlienRaidersGame::smartBomb() {
  for (uint8_t i = 0; i < ENEMIES; i++) {
    if (enemies_[i].active) {
      if (enemies_[i].hp <= 1 && !enemies_[i].boss) {
        enemies_[i].active = false;
        score_++;
      } else if (enemies_[i].hp > 0) {
        enemies_[i].hp--;
      }
    }
  }
  for (uint8_t i = 0; i < ENEMY_SHOTS; i++) enemyShots_[i].active = false;
  if (score_ > bestScore_) {
    bestScore_ = score_;
    saveBestScore();
  }
}

void AlienRaidersGame::loseHealth() {
  if (shieldHits_ > 0) {
    shieldHits_--;
    return;
  }
  if (health_ > 0) health_--;
  if (health_ == 0) endApp();
}

void AlienRaidersGame::fireBossShot(int8_t dy) {
  for (uint8_t i = 0; i < ENEMY_SHOTS; i++) {
    if (!enemyShots_[i].active) {
      enemyShots_[i].active = true;
      enemyShots_[i].x = static_cast<float>(laneX(1));
      enemyShots_[i].y = bossActive_ ? enemies_[0].x + 34.0f : static_cast<float>(PLAY_TOP + 20);
      enemyShots_[i].dy = dy;
      return;
    }
  }
}

void AlienRaidersGame::drawRunning(TFT_eSPI& tft) {
  setPortrait(tft);
  TDisplayFramebuffer::draw(tft, PORTRAIT_W, PORTRAIT_H, [&](auto& canvas) {
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextDatum(TL_DATUM);

    if (introActive_) {
      drawIntroFrame(canvas, introTimerMs_, laneX(0), laneX(1), laneX(2));
      return;
    }

    const String stat = String(score_) + " W" + String(weaponLevel_) + " H" + String(health_);
    drawPortraitHeader(canvas, "Raiders", TFT_CYAN, stat);
    for (uint8_t lane = 0; lane < LANES; lane++) {
      canvas.drawFastVLine(laneX(lane), PLAY_TOP, PLAY_BOTTOM - PLAY_TOP, TFT_DARKGREY);
    }
    canvas.fillRect(13, SHIP_Y + 20, PORTRAIT_W - 26, 10, TFT_DARKGREY);
    canvas.fillRect(laneX(0) - 9, SHIP_Y + 23, 18, 5, TFT_BLUE);
    canvas.fillRect(laneX(1) - 9, SHIP_Y + 23, 18, 5, TFT_BLUE);
    canvas.fillRect(laneX(2) - 9, SHIP_Y + 23, 18, 5, TFT_BLUE);
    const int shipX = laneX(shipLane_);
    canvas.drawTriangle(shipX, SHIP_Y - 15, shipX - 11, SHIP_Y + 10, shipX + 11, SHIP_Y + 10, TFT_WHITE);
    canvas.fillTriangle(shipX, SHIP_Y - 11, shipX - 7, SHIP_Y + 7, shipX + 7, SHIP_Y + 7, TFT_CYAN);
    if (shieldHits_ > 0) canvas.drawCircle(shipX, SHIP_Y, 15, TFT_GREEN);
    if (shieldHits_ > 2) canvas.drawCircle(shipX, SHIP_Y, 19, TFT_GREEN);
    if (shieldHits_ > 5) canvas.drawCircle(shipX, SHIP_Y, 23, TFT_GREEN);
    for (uint8_t i = 0; i < ENEMIES; i++) {
      if (enemies_[i].active) drawEnemyShape(canvas, enemies_[i], laneX(0), laneX(1), laneX(2), laneX(enemies_[i].lane));
    }
    for (uint8_t i = 0; i < SHOTS; i++) {
      if (shots_[i].active) {
        const int x = static_cast<int>(shots_[i].x);
        const int y = static_cast<int>(shots_[i].y);
        canvas.drawFastVLine(x, y - 6, 8, TFT_YELLOW);
        canvas.drawPixel(x, y - 7, TFT_WHITE);
      }
    }
    for (uint8_t i = 0; i < ENEMY_SHOTS; i++) {
      if (!enemyShots_[i].active) continue;
      const int x = static_cast<int>(enemyShots_[i].x);
      const int y = static_cast<int>(enemyShots_[i].y);
      canvas.drawLine(x, y - 2, x, y + 7, TFT_RED);
      canvas.drawPixel(x, y + 8, TFT_YELLOW);
    }
    for (uint8_t i = 0; i < POWERS; i++) {
      if (!powers_[i].active) continue;
      drawPowerShape(canvas, powers_[i], laneX(powers_[i].lane));
    }
  });
}

void AlienRaidersGame::drawEnemy(TFT_eSPI& tft, const Enemy& enemy) const {
  drawEnemyShape(tft, enemy, laneX(0), laneX(1), laneX(2), laneX(enemy.lane));
}

void AlienRaidersGame::drawPower(TFT_eSPI& tft, const Power& power) const {
  drawPowerShape(tft, power, laneX(power.lane));
}

void AlienRaidersGame::drawIntro(TFT_eSPI& tft) const {
  drawIntroFrame(tft, introTimerMs_, laneX(0), laneX(1), laneX(2));
}

void AlienRaidersGame::drawStart(TFT_eSPI& tft) {
  clearPortrait(tft);
  loadBestScore();
  if (showStartPromptPage()) {
    drawPortraitHeader(tft, appTitle(), TFT_CYAN);
    drawCentered(tft, "Press", 86, 2, TFT_WHITE);
    drawCentered(tft, "to Start", 111, 2, TFT_LIGHTGREY);
    drawPortraitFooter(tft, "B1 start");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);

    drawPortraitHeader(tft, "Top Kills", TFT_YELLOW);
    drawCentered(tft, "Best", 63, 2, TFT_LIGHTGREY);
    drawCentered(tft, bestScore_ == 0 ? String("--") : String(bestScore_), 94, 3, TFT_YELLOW);
    drawCentered(tft, bestScore_ == 0 ? String("") : String(initials), 139, 2, TFT_LIGHTGREY);
    drawPortraitFooter(tft, "Best defense");
  } else {
    drawPortraitHeader(tft, appTitle(), TFT_CYAN);
    tft.fillRoundRect(34, 174, 67, 36, 4, TFT_DARKGREY);
    tft.drawRoundRect(34, 174, 67, 36, 4, TFT_LIGHTGREY);
    tft.fillTriangle(laneX(1), 132, laneX(1) - 12, 157, laneX(1) + 12, 157, TFT_CYAN);
    tft.drawTriangle(laneX(1), 132, laneX(1) - 12, 157, laneX(1) + 12, 157, TFT_WHITE);
    tft.fillTriangle(laneX(0), 58, laneX(0) - 11, 36, laneX(0) + 11, 36, TFT_RED);
    tft.fillTriangle(laneX(2), 84, laneX(2) - 11, 62, laneX(2) + 11, 62, TFT_ORANGE);
    drawPortraitFooter(tft, "B1 left  B2 right");
  }
}

void AlienRaidersGame::drawEnd(TFT_eSPI& tft) {
  clearPortrait(tft);
  drawPortraitHeader(tft, "Defeated", TFT_RED);
  drawCentered(tft, "Kills", 64, 1, TFT_LIGHTGREY);
  drawCentered(tft, String(score_), 84, 3, TFT_CYAN);
  String best = String(bestScore_);
  String initialsLabel;
  if (bestScore_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    initialsLabel = initials;
  }
  drawCentered(tft, "Best", 130, 1, TFT_LIGHTGREY);
  drawCentered(tft, best, 146, 2, TFT_YELLOW);
  if (initialsLabel.length() > 0) {
    drawCentered(tft, initialsLabel, 172, 2, TFT_LIGHTGREY);
  }
  drawPortraitFooter(tft, "B1 retry  Hold menu");
}

int AlienRaidersGame::laneX(uint8_t lane) const {
  return LANE_LEFT + static_cast<int>(lane) * LANE_STEP;
}

void AlienRaidersGame::loadBestScore() {
  if (bestLoaded_) return;
  alienPrefs.begin("aliens", true);
  bestScore_ = alienPrefs.getUShort("best", 0);
  bestInitials_ = alienPrefs.getUShort("init", PlayerProfile::defaultInitials());
  alienPrefs.end();
  bestLoaded_ = true;
}

void AlienRaidersGame::saveBestScore() {
  bestInitials_ = PlayerProfile::loadInitials();
  alienPrefs.begin("aliens", false);
  alienPrefs.putUShort("best", bestScore_);
  alienPrefs.putUShort("init", bestInitials_);
  alienPrefs.end();
}
