#include "AlienRaidersGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
Preferences alienPrefs;
}

AlienRaidersGame::AlienRaidersGame(uint32_t width, uint32_t height, uint32_t left)
    : App("Alien Raiders", width, height), left_(left) {}

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
  score_ = 0;
  for (uint8_t i = 0; i < ENEMIES; i++) enemies_[i].active = false;
  for (uint8_t i = 0; i < SHOTS; i++) shots_[i].active = false;
  for (uint8_t i = 0; i < ENEMY_SHOTS; i++) enemyShots_[i].active = false;
  for (uint8_t i = 0; i < POWERS; i++) powers_[i].active = false;
}

void AlienRaidersGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
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

  if (input.click) shipLane_ = (shipLane_ + 1) % LANES;

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
    const float base = enemies_[i].boss ? 5.5f : 17.0f + min<uint16_t>(20, score_ / 4);
    enemies_[i].x -= (base - (enemies_[i].boss ? 0.0f : (enemies_[i].maxHp - 1) * 3.0f)) * dt;
    if (enemies_[i].x < 5.0f) {
      if (enemies_[i].boss) {
        bossActive_ = false;
        nextBossScore_ += 100;
      }
      enemies_[i].active = false;
      loseHealth();
    }
  }

  for (uint8_t i = 0; i < SHOTS; i++) {
    if (!shots_[i].active) continue;
    shots_[i].x += 45.0f * dt;
    shots_[i].lane += shots_[i].dy;
    shots_[i].y += static_cast<float>(shots_[i].dy) * 7.0f * dt;
    if (shots_[i].x > width || shots_[i].y < 2 || shots_[i].y > height - 2) shots_[i].active = false;
  }

  for (uint8_t i = 0; i < ENEMY_SHOTS; i++) {
    if (!enemyShots_[i].active) continue;
    enemyShots_[i].x -= 34.0f * dt;
    enemyShots_[i].y += static_cast<float>(enemyShots_[i].dy) * 6.0f * dt;
    if (enemyShots_[i].x < 8.0f && enemyShots_[i].x > 1.0f &&
        abs(static_cast<int>(enemyShots_[i].y) - laneY(shipLane_)) <= 4) {
      enemyShots_[i].active = false;
      loseHealth();
    } else if (enemyShots_[i].x < 0.0f || enemyShots_[i].y < 5.0f || enemyShots_[i].y > 35.0f) {
      enemyShots_[i].active = false;
    }
  }

  for (uint8_t i = 0; i < POWERS; i++) {
    if (!powers_[i].active) continue;
    powers_[i].x -= 18.0f * dt;
    if (powers_[i].x < 8.0f && powers_[i].lane == shipLane_) {
      applyPower(powers_[i].type);
      powers_[i].active = false;
    } else if (powers_[i].x < 0.0f) {
      powers_[i].active = false;
    }
  }

  for (uint8_t e = 0; e < ENEMIES; e++) {
    if (!enemies_[e].active) continue;
    for (uint8_t s = 0; s < SHOTS; s++) {
      if (!shots_[s].active) continue;
      const int shotLane = constrain(static_cast<int>(shots_[s].lane), 0, LANES - 1);
      const bool bossHit = enemies_[e].boss && shots_[s].x >= enemies_[e].x && shots_[s].x <= enemies_[e].x + 12.0f &&
                           shots_[s].y >= 8 && shots_[s].y <= 32;
      const bool laneHit = shotLane == enemies_[e].lane && shots_[s].x >= enemies_[e].x && shots_[s].x <= enemies_[e].x + 5.0f;
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
      enemies_[i].x = static_cast<float>(width - 6);
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
  enemies_[0].x = static_cast<float>(width - 14);
  enemies_[0].lane = 1;
  enemies_[0].hp = 28 + min<uint16_t>(120, score_ / 4);
  enemies_[0].maxHp = enemies_[0].hp;
  bossActive_ = true;
  bossShotTimerMs_ = 250;
}

void AlienRaidersGame::fireShot(uint8_t lane, int8_t dy) {
  for (uint8_t i = 0; i < SHOTS; i++) {
    if (!shots_[i].active) {
      shots_[i].active = true;
      shots_[i].x = 9.0f + (doubleMs_ > 0 ? static_cast<float>(i % 2) : 0.0f);
      shots_[i].lane = lane;
      shots_[i].y = static_cast<float>(laneY(lane) + 1);
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
      enemyShots_[i].x = 50.0f;
      enemyShots_[i].y = static_cast<float>(laneY(1));
      enemyShots_[i].dy = dy;
      return;
    }
  }
}

void AlienRaidersGame::drawRunning(U8G2& u8g2) {
  if (introActive_) {
    drawIntro(u8g2);
    return;
  }

  u8g2.drawFrame(left_, 0, width, height);
  u8g2.drawTriangle(left_ + 9, laneY(shipLane_) + 1, left_ + 3, laneY(shipLane_) - 3, left_ + 3, laneY(shipLane_) + 5);
  if (shieldHits_ > 0) u8g2.drawCircle(left_ + 6, laneY(shipLane_) + 1, 5);
  if (shieldHits_ > 2) u8g2.drawCircle(left_ + 6, laneY(shipLane_) + 1, 7);
  if (shieldHits_ > 5) u8g2.drawCircle(left_ + 6, laneY(shipLane_) + 1, 9);
  for (uint8_t i = 0; i < ENEMIES; i++) if (enemies_[i].active) drawEnemy(u8g2, enemies_[i]);
  for (uint8_t i = 0; i < SHOTS; i++) if (shots_[i].active) u8g2.drawPixel(left_ + static_cast<int>(shots_[i].x), static_cast<int>(shots_[i].y));
  for (uint8_t i = 0; i < ENEMY_SHOTS; i++) {
    if (!enemyShots_[i].active) continue;
    const int x = left_ + static_cast<int>(enemyShots_[i].x);
    const int y = static_cast<int>(enemyShots_[i].y);
    u8g2.drawLine(x, y, x + 2, y);
  }
  for (uint8_t i = 0; i < POWERS; i++) {
    if (!powers_[i].active) continue;
    drawPower(u8g2, powers_[i]);
  }
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 2, 6);
  u8g2.print(score_);
  u8g2.setCursor(left_ + 45, 6);
  u8g2.print("W");
  u8g2.print(weaponLevel_);
  u8g2.print(" H");
  u8g2.print(health_);
}

void AlienRaidersGame::drawEnemy(U8G2& u8g2, const Enemy& enemy) const {
  const int x = left_ + static_cast<int>(enemy.x);
  const int y = laneY(enemy.lane);
  if (enemy.boss) {
    u8g2.drawFrame(x, 9, 12, 22);
    u8g2.drawTriangle(x, 20, x + 12, 12, x + 12, 28);
    if (enemy.hp > 0) u8g2.drawFrame(x - 2, 7, 16, 26);
    if (enemy.hp > enemy.maxHp / 3) u8g2.drawFrame(x - 4, 5, 20, 30);
    if (enemy.hp > (enemy.maxHp * 2) / 3) u8g2.drawFrame(x - 6, 3, 24, 34);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(x + 1, 22);
    u8g2.print(static_cast<int>(enemy.hp));
    return;
  }
  u8g2.drawTriangle(x, y, x + 6, y - 4, x + 6, y + 4);
  const uint8_t shieldLayers = enemy.hp > 1 ? enemy.hp - 1 : 0;
  if (shieldLayers >= 1) u8g2.drawCircle(x + 5, y, 5);
  if (shieldLayers >= 2) u8g2.drawCircle(x + 5, y, 7);
  if (shieldLayers >= 3) u8g2.drawCircle(x + 5, y, 9);
  if (shieldLayers >= 4) {
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(x + 1, y + 2);
    u8g2.print(static_cast<int>(enemy.hp));
  }
}

void AlienRaidersGame::drawPower(U8G2& u8g2, const Power& power) const {
  const int x = left_ + static_cast<int>(power.x);
  const int y = laneY(power.lane);
  if (power.type == 5) {
    u8g2.drawDisc(x + 2, y + 1, 3);
    u8g2.drawLine(x + 2, y - 3, x + 4, y - 6);
    u8g2.drawPixel(x + 5, y - 6);
    return;
  }
  u8g2.drawCircle(x + 2, y, 3);
  u8g2.drawLine(x, y, x + 4, y);
  u8g2.drawLine(x + 2, y - 2, x + 2, y + 2);
}

void AlienRaidersGame::drawIntro(U8G2& u8g2) const {
  u8g2.drawFrame(left_, 0, width, height);
  const uint16_t t = introTimerMs_;
  const int stationX = left_ + 4;
  const int shipX = left_ + 15 + min<int>(28, t / 55);
  const int shipY = 22;
  const int enemyX = left_ + 58;

  u8g2.drawFrame(stationX, 13, 13, 18);
  u8g2.drawBox(stationX + 3, 17, 4, 3);
  u8g2.drawBox(stationX + 3, 25, 4, 3);
  u8g2.drawLine(stationX + 12, 20, stationX + 17, 22);
  u8g2.drawLine(stationX + 12, 25, stationX + 17, 22);
  u8g2.drawTriangle(shipX + 7, shipY, shipX, shipY - 3, shipX, shipY + 3);
  if (t > 1000) {
    u8g2.drawTriangle(enemyX, 18, enemyX + 6, 14, enemyX + 6, 22);
    u8g2.drawTriangle(enemyX + 5, 29, enemyX + 10, 26, enemyX + 10, 32);
    u8g2.drawStr(left_ + 3, 8, "Defend Station");
  } else {
    u8g2.drawStr(left_ + 3, 8, "Launch");
  }
}

void AlienRaidersGame::drawStart(U8G2& u8g2) {
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
    u8g2.drawStr(3, 10, "Top Kills");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(3, 24);
    if (bestScore_ == 0) u8g2.print("--");
    else {
      u8g2.print(initials);
      u8g2.print(" ");
      u8g2.print(bestScore_);
    }
  } else {
    const int stationX = 4;
    u8g2.drawFrame(stationX, 14, 13, 17);
    u8g2.drawBox(stationX + 3, 18, 4, 3);
    u8g2.drawBox(stationX + 3, 25, 4, 3);
    u8g2.drawLine(stationX + 12, 20, stationX + 17, 22);
    u8g2.drawLine(stationX + 12, 25, stationX + 17, 22);
    u8g2.drawTriangle(33, 22, 26, 19, 26, 25);
    u8g2.drawTriangle(60, 18, 66, 14, 66, 22);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 9, appTitle());
  }
}

void AlienRaidersGame::drawEnd(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, "Defeated");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(3, 20);
  u8g2.print("Kills ");
  u8g2.print(score_);
  u8g2.setCursor(3, 29);
  u8g2.print("Best ");
  u8g2.print(bestScore_);
  if (bestScore_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.print(" ");
    u8g2.print(initials);
  }
  u8g2.drawStr(3, 38, "Tap retry Hold menu");
}

int AlienRaidersGame::laneY(uint8_t lane) const {
  return 10 + lane * 10;
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
