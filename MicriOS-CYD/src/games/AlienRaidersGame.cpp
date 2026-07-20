#include "AlienRaidersGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../CydHardware.h"
#include "../../PlayerProfile.h"
#include "../../CydFramebuffer.h"
namespace {
Preferences alienPrefs;
constexpr int PORTRAIT_W = 320;
constexpr int PORTRAIT_H = 208;
constexpr int HUD_H = 27;
// Match City Racer's long-axis layout: the player sits at the left and
// attackers approach from the right through three horizontal lanes.
constexpr int PLAYER_X = 43;
constexpr int PLAY_TOP = HUD_H + 2;
constexpr int PLAY_BOTTOM = PORTRAIT_H - 2;
constexpr int PLAY_RIGHT = PORTRAIT_W - 4;
constexpr int LANE_H = (PLAY_BOTTOM - PLAY_TOP) / 3;
constexpr float ENEMY_BASE_SPEED = 62.0f;
constexpr float ENEMY_MAX_BONUS_SPEED = 52.0f;
constexpr float SPREAD_Y_SPEED = 44.0f;
constexpr float BOSS_SPEED = 9.0f;
constexpr float BOSS_NEAR_X = PLAYER_X + 98.0f;
constexpr float BOSS_FAR_X = PORTRAIT_W - 48.0f;

void setLandscape(TFT_eSPI& tft) {
  CydHardware::applyDisplayOrientation(tft);
}

void clearLandscape(TFT_eSPI& tft) {
  setLandscape(tft);
  tft.fillRect(0, 0, PORTRAIT_W, PORTRAIT_H, TFT_BLACK);
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

template <typename Canvas>
void drawHealthCross(Canvas& canvas, int x, int y, int size,
                     uint16_t color = TFT_RED) {
  const int arm = max(1, size / 3);
  canvas.fillRect(x - arm / 2, y - size / 2, arm, size, color);
  canvas.fillRect(x - size / 2, y - arm / 2, size, arm, color);
}

// A compact, original spacedock silhouette inspired by the broad upper dock,
// central neck and lower spherical module of classic orbital stations.
template <typename Canvas>
void drawSpacedock(Canvas& canvas, int cx, int top, int w, int h,
                   bool damaged = false) {
  const int upperY = top + h * 12 / 100;
  const int upperH = max(6, h * 10 / 100);
  const int neckW = max(8, w * 24 / 100);
  const int midY = top + h * 35 / 100;
  const int lowerY = top + h * 54 / 100;
  const int lowerW = max(14, w * 58 / 100);
  const int stemY = top + h * 69 / 100;
  const int sphereY = top + h * 88 / 100;
  const uint16_t hull = damaged ? TFT_DARKGREY : TFT_LIGHTGREY;
  const uint16_t shadow = TFT_DARKGREY;

  // Crown and broad docking saucer.
  canvas.drawFastVLine(cx, top, max(4, h * 9 / 100), TFT_WHITE);
  canvas.drawFastVLine(cx - w / 7, top + 3, max(3, h * 6 / 100),
                       TFT_LIGHTGREY);
  canvas.drawFastVLine(cx + w / 7, top + 2, max(3, h * 7 / 100),
                       TFT_LIGHTGREY);
  canvas.fillTriangle(cx, top + 3, cx - w / 3, upperY + upperH,
                      cx + w / 3, upperY + upperH, shadow);
  canvas.fillRoundRect(cx - w / 2, upperY, w, upperH, upperH / 2, hull);
  canvas.drawFastHLine(cx - w / 2 + 3, upperY + upperH / 2, w - 6,
                       TFT_CYAN);
  canvas.fillRoundRect(cx - w * 3 / 8, upperY + upperH,
                       w * 3 / 4, max(5, h * 9 / 100), 3, shadow);

  // Neck, lower operations body and long central stem.
  canvas.fillRect(cx - neckW / 2, midY, neckW, h * 17 / 100, hull);
  canvas.drawFastHLine(cx - neckW / 2, midY + h * 6 / 100, neckW,
                       TFT_CYAN);
  canvas.fillRoundRect(cx - lowerW / 2, lowerY, lowerW, h * 13 / 100, 5,
                       hull);
  canvas.drawFastHLine(cx - lowerW / 2 + 3, lowerY + h * 6 / 100,
                       lowerW - 6, TFT_BLUE);
  canvas.fillRect(cx - neckW / 2, stemY, neckW, h * 13 / 100, shadow);
  canvas.drawFastHLine(cx - neckW / 2, stemY + h * 6 / 100, neckW,
                       TFT_CYAN);
  canvas.fillCircle(cx, sphereY, max(5, w * 17 / 100), hull);
  canvas.drawCircle(cx, sphereY, max(5, w * 17 / 100), TFT_WHITE);

  // Tiny lit windows communicate scale without needing text.
  for (int x = cx - w / 3; x <= cx + w / 3; x += max(4, w / 9)) {
    canvas.drawPixel(x, upperY + 2, TFT_BLUE);
  }
  for (int x = cx - lowerW / 3; x <= cx + lowerW / 3;
       x += max(4, lowerW / 5)) {
    canvas.drawPixel(x, lowerY + 3, TFT_CYAN);
  }

  if (damaged) {
    canvas.fillTriangle(cx - w / 2, upperY + 1, cx - w / 3,
                        upperY + upperH, cx - w / 5, upperY, TFT_BLACK);
    canvas.fillRect(cx + lowerW / 6, lowerY, lowerW / 4,
                    max(3, h * 6 / 100), TFT_BLACK);
  }
}

template <typename Canvas>
void drawStationHealth(Canvas& canvas, uint8_t health) {
  for (uint8_t i = 0; i < health; ++i) {
    const int x = 6 + (i % 2) * 10;
    const int y = PLAY_TOP + 10 + (i / 2) * 18;
    drawHealthCross(canvas, x, y, 7, TFT_RED);
  }
}

template <typename Canvas>
void drawExplosion(Canvas& canvas, int x, int y, uint16_t ageMs,
                   bool large = false) {
  const uint8_t frame = min<uint8_t>(4, ageMs / 70);
  const int radius = (large ? 8 : 4) + frame * (large ? 5 : 3);
  canvas.fillCircle(x, y, max(2, radius / 3), TFT_WHITE);
  canvas.drawCircle(x, y, max(3, radius * 2 / 3), TFT_YELLOW);
  canvas.drawCircle(x, y, radius, frame >= 3 ? TFT_RED : TFT_ORANGE);
  canvas.drawLine(x - radius - 3, y, x + radius + 3, y, TFT_ORANGE);
  canvas.drawLine(x, y - radius - 3, x, y + radius + 3, TFT_YELLOW);
  canvas.drawLine(x - radius, y - radius, x + radius, y + radius, TFT_RED);
  canvas.drawLine(x - radius, y + radius, x + radius, y - radius, TFT_RED);
}

template <typename Canvas, typename EnemyLike>
void drawEnemyShape(Canvas& canvas, const EnemyLike& enemy, int laneY0,
                    int laneY1, int laneY2, int laneYEnemy) {
  const int x = static_cast<int>(enemy.x);
  if (enemy.boss) {
    const int top = laneY0 - 18;
    const int bottom = laneY2 + 18;
    canvas.fillRoundRect(x - 15, top + 12, 30, bottom - top - 24, 5,
                         TFT_PURPLE);
    canvas.fillTriangle(x - 34, laneY1, x + 14, top, x + 14, bottom,
                        TFT_MAGENTA);
    canvas.drawTriangle(x - 34, laneY1, x + 14, top, x + 14, bottom,
                        TFT_WHITE);
    if (enemy.hp > 0)
      canvas.drawRoundRect(x - 39, top - 3, 58, bottom - top + 6, 10,
                           TFT_BLUE);
    if (enemy.hp > enemy.maxHp / 3)
      canvas.drawRoundRect(x - 43, top - 7, 66, bottom - top + 14, 12,
                           TFT_CYAN);
    if (enemy.hp > (enemy.maxHp * 2) / 3)
      canvas.drawRoundRect(x - 47, top - 11, 74, bottom - top + 22, 14,
                           TFT_WHITE);

    // Each cross-box represents ten hit points. The active partial box
    // shrinks from 9x9 down to 3x3 before disappearing.
    const uint8_t fullBoxes = enemy.hp / 10;
    const uint8_t partialHealth = enemy.hp % 10;
    const uint8_t visibleBoxes = fullBoxes + (partialHealth > 0 ? 1 : 0);
    const uint8_t maxBoxes = (enemy.maxHp + 9) / 10;
    constexpr int boxSize = 9;
    constexpr int boxStep = 11;
    int startY = laneY1 - static_cast<int>(maxBoxes) * boxStep / 2;
    if (startY < PLAY_TOP + 2) startY = PLAY_TOP + 2;
    if (startY + static_cast<int>(maxBoxes) * boxStep > PLAY_BOTTOM - 2) {
      startY = PLAY_BOTTOM - 2 - static_cast<int>(maxBoxes) * boxStep;
    }
    const int healthX = min(PORTRAIT_W - boxSize - 2, x + 25);
    for (uint8_t i = 0; i < visibleBoxes; ++i) {
      const uint8_t amount =
          i < fullBoxes ? 10 : static_cast<uint8_t>(partialHealth);
      const int size = amount == 10 ? boxSize : max(3, (boxSize * amount + 9) / 10);
      const int bx = healthX + (boxSize - size) / 2;
      const int by = startY + static_cast<int>(i) * boxStep +
                     (boxSize - size) / 2;
      canvas.drawRect(bx, by, size, size, TFT_RED);
      if (size >= 5) drawHealthCross(canvas, bx + size / 2, by + size / 2,
                                     size - 2, TFT_RED);
    }
    return;
  }

  canvas.fillTriangle(x - 13, laneYEnemy, x + 9, laneYEnemy - 11, x + 9,
                      laneYEnemy + 11,
                      enemy.maxHp > 1 ? TFT_ORANGE : TFT_RED);
  canvas.drawTriangle(x - 13, laneYEnemy, x + 9, laneYEnemy - 11, x + 9,
                      laneYEnemy + 11, TFT_WHITE);
  const uint8_t shieldLayers = enemy.hp > 1 ? enemy.hp - 1 : 0;
  for (uint8_t layer = 0; layer < shieldLayers; ++layer) {
    // Keep the original three shields visually distinct, then pack later
    // one-pixel rings tightly enough that a seven-HP enemy still fits a lane.
    const int radius = layer < 3 ? 14 + layer * 4 : 25 + (layer - 3) * 3;
    const uint16_t color =
        layer == 0 ? TFT_CYAN
                   : layer == 1 ? TFT_BLUE
                                : layer == 2 ? TFT_LIGHTGREY
                                             : (layer & 1) ? TFT_MAGENTA
                                                           : TFT_SKYBLUE;
    canvas.drawCircle(x, laneYEnemy, radius, color);
  }
}

template <typename Canvas, typename PowerLike>
void drawPowerShape(Canvas& canvas, const PowerLike& power, int laneYPower) {
  const int x = static_cast<int>(power.x);
  if (power.type == 5) {
    canvas.fillCircle(x, laneYPower, 8, TFT_DARKGREY);
    canvas.drawCircle(x, laneYPower, 8, TFT_WHITE);
    canvas.drawLine(x + 6, laneYPower - 6, x + 12, laneYPower - 12,
                    TFT_WHITE);
    canvas.fillCircle(x + 14, laneYPower - 13, 2, TFT_ORANGE);
    return;
  }
  canvas.drawCircle(x, laneYPower, 8, TFT_GREEN);
  canvas.drawLine(x - 6, laneYPower, x + 6, laneYPower, TFT_GREEN);
  canvas.drawLine(x, laneYPower - 6, x, laneYPower + 6, TFT_GREEN);
}

template <typename Canvas>
void drawIntroFrame(Canvas& canvas, uint16_t timerMs, int lane0, int lane1,
                    int lane2) {
  drawPortraitHeader(canvas, "Alien Raiders", TFT_CYAN);
  const int shipX = 64 + min<int>(102, timerMs / 14);

  drawSpacedock(canvas, 31, PLAY_TOP + 2, 54,
                PLAY_BOTTOM - PLAY_TOP - 5);
  canvas.fillTriangle(shipX + 15, lane1, shipX - 10, lane1 - 11,
                      shipX - 10, lane1 + 11, TFT_CYAN);
  canvas.drawTriangle(shipX + 15, lane1, shipX - 10, lane1 - 11,
                      shipX - 10, lane1 + 11, TFT_WHITE);
  if (timerMs > 1000) {
    canvas.fillTriangle(259, lane0, 272, lane0 - 6, 272, lane0 + 6,
                        TFT_RED);
    canvas.fillTriangle(279, lane2, 292, lane2 - 6, 292, lane2 + 6,
                        TFT_ORANGE);
    drawCentered(canvas, "Defend Station", 151, 2, TFT_YELLOW);
  } else {
    drawCentered(canvas, "Launch", 151, 2, TFT_LIGHTGREY);
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
  stationImpactMs_ = 0;
  destructionTimerMs_ = 0;
  nextBossScore_ = 100;
  introActive_ = true;
  destructionActive_ = false;
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

  if (destructionActive_) {
    destructionTimerMs_ += deltaMs;
    if (destructionTimerMs_ >= 1260) {
      destructionActive_ = false;
      endApp();
    }
    return;
  }

  if (stationImpactMs_ > 0) {
    stationImpactMs_ =
        deltaMs >= stationImpactMs_ ? 0 : stationImpactMs_ - deltaMs;
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
      if (enemies_[i].x >= BOSS_FAR_X) {
        enemies_[i].x = BOSS_FAR_X;
        bossDirection_ = -1;
      } else if (enemies_[i].x <= BOSS_NEAR_X) {
        enemies_[i].x = BOSS_NEAR_X;
        bossDirection_ = 1;
      }
      continue;
    }

    const float base = ENEMY_BASE_SPEED + min<uint16_t>(ENEMY_MAX_BONUS_SPEED, score_ / 3);
    enemies_[i].x -= (base - (enemies_[i].maxHp - 1) * 5.0f) * dt;
    if (enemies_[i].x < PLAYER_X - 18.0f) {
      triggerStationImpact(enemies_[i].lane);
      enemies_[i].active = false;
      loseHealth();
    }
  }
  if (destructionActive_) return;

  for (uint8_t i = 0; i < SHOTS; i++) {
    if (!shots_[i].active) continue;
    shots_[i].x += 180.0f * dt;
    shots_[i].y += static_cast<float>(shots_[i].dy) * SPREAD_Y_SPEED * dt;
    if (shots_[i].x > PLAY_RIGHT || shots_[i].y < PLAY_TOP ||
        shots_[i].y > PLAY_BOTTOM) {
      shots_[i].active = false;
    }
  }

  for (uint8_t i = 0; i < ENEMY_SHOTS; i++) {
    if (!enemyShots_[i].active) continue;
    enemyShots_[i].x -= 104.0f * dt;
    enemyShots_[i].y += static_cast<float>(enemyShots_[i].dy) * 30.0f * dt;
    if (enemyShots_[i].x > PLAYER_X - 14.0f &&
        enemyShots_[i].x < PLAYER_X + 15.0f &&
        abs(static_cast<int>(enemyShots_[i].y) - laneY(shipLane_)) <= 12) {
      enemyShots_[i].active = false;
      loseHealth();
    } else if (enemyShots_[i].x < 0.0f || enemyShots_[i].y < PLAY_TOP ||
               enemyShots_[i].y > PLAY_BOTTOM) {
      enemyShots_[i].active = false;
    }
  }
  if (destructionActive_) return;

  for (uint8_t i = 0; i < POWERS; i++) {
    if (!powers_[i].active) continue;
    powers_[i].x -= 54.0f * dt;
    if (powers_[i].x > PLAYER_X - 14.0f &&
        powers_[i].x < PLAYER_X + 15.0f &&
        powers_[i].lane == shipLane_) {
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
      const bool bossHit =
          enemies_[e].boss &&
          shots_[s].x >= enemies_[e].x - 38.0f &&
          shots_[s].x <= enemies_[e].x + 20.0f &&
          shots_[s].y >= laneY(0) - 18 && shots_[s].y <= laneY(2) + 18;
      const bool laneHit = !enemies_[e].boss &&
                           abs(static_cast<int>(shots_[s].y) -
                               laneY(enemies_[e].lane)) <= 13 &&
                           shots_[s].x >= enemies_[e].x - 15.0f &&
                           shots_[s].x <= enemies_[e].x + 13.0f;
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
      enemies_[i].x = static_cast<float>(PLAY_RIGHT + 12);
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
  enemies_[0].x = BOSS_FAR_X;
  enemies_[0].lane = 1;
  enemies_[0].hp = 28 + min<uint16_t>(120, score_ / 4);
  enemies_[0].maxHp = enemies_[0].hp;
  bossActive_ = true;
  bossDirection_ = -1;
  bossShotTimerMs_ = 250;
}

void AlienRaidersGame::fireShot(uint8_t lane, int8_t dy) {
  for (uint8_t i = 0; i < SHOTS; i++) {
    if (!shots_[i].active) {
      shots_[i].active = true;
      shots_[i].x = static_cast<float>(PLAYER_X + 17);
      shots_[i].lane = lane;
      const bool doubleShot = doubleMs_ > 0 || weaponLevel_ >= 2;
      shots_[i].y = static_cast<float>(
          laneY(lane) + (doubleShot ? static_cast<int>((i % 2) * 6 - 3) : 0));
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
  if (destructionActive_) return;
  if (shieldHits_ > 0) {
    shieldHits_--;
    return;
  }
  if (health_ > 0) health_--;
  if (health_ == 0) startStationDestruction();
}

void AlienRaidersGame::triggerStationImpact(uint8_t lane) {
  stationImpactLane_ = min<uint8_t>(LANES - 1, lane);
  stationImpactMs_ = 280;
}

void AlienRaidersGame::startStationDestruction() {
  destructionActive_ = true;
  destructionTimerMs_ = 0;
  stationImpactMs_ = 0;
  bossActive_ = false;
  for (uint8_t i = 0; i < ENEMIES; ++i) enemies_[i].active = false;
  for (uint8_t i = 0; i < SHOTS; ++i) shots_[i].active = false;
  for (uint8_t i = 0; i < ENEMY_SHOTS; ++i) enemyShots_[i].active = false;
  for (uint8_t i = 0; i < POWERS; ++i) powers_[i].active = false;
}

void AlienRaidersGame::fireBossShot(int8_t dy) {
  for (uint8_t i = 0; i < ENEMY_SHOTS; i++) {
    if (!enemyShots_[i].active) {
      enemyShots_[i].active = true;
      enemyShots_[i].x =
          bossActive_ ? enemies_[0].x - 34.0f : BOSS_FAR_X;
      enemyShots_[i].y = static_cast<float>(laneY(1));
      enemyShots_[i].dy = dy;
      return;
    }
  }
}

void AlienRaidersGame::drawRunning(TFT_eSPI& tft) {
  setLandscape(tft);
  CydFramebuffer::draw(tft, PORTRAIT_W, PORTRAIT_H, [&](auto& canvas) {
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextDatum(TL_DATUM);

    if (introActive_) {
      drawIntroFrame(canvas, introTimerMs_, laneY(0), laneY(1), laneY(2));
      return;
    }

    const String stat = String("K") + String(score_) + " W" +
                        String(weaponLevel_);
    drawPortraitHeader(canvas,
                       destructionActive_ ? "Station Critical" : "Raiders",
                       destructionActive_ ? TFT_RED : TFT_CYAN, stat);
    for (uint8_t lane = 1; lane < LANES; lane++) {
      const int separatorY = PLAY_TOP + lane * LANE_H;
      for (int x = 70; x < PORTRAIT_W; x += 24) {
        canvas.drawFastHLine(x, separatorY, 12, TFT_DARKGREY);
      }
    }

    drawSpacedock(canvas, 20, PLAY_TOP + 4, 36,
                  PLAY_BOTTOM - PLAY_TOP - 8,
                  destructionActive_ && destructionTimerMs_ > 330);

    if (destructionActive_) {
      const uint16_t age = destructionTimerMs_;
      if (age < 970) {
        const uint8_t lane = min<uint8_t>(2, age / 260);
        drawExplosion(canvas, 22, laneY(lane), age % 260, true);
      }
      if (age > 280 && age < 1100) {
        drawExplosion(canvas, 30, laneY(1), age - 280, true);
      }
      if (age > 560) {
        const int drift = min<int>(36, (age - 560) / 14);
        canvas.drawLine(18 - drift, laneY(0) - drift / 2, 5,
                        laneY(0) - drift, TFT_LIGHTGREY);
        canvas.drawLine(27 + drift, laneY(2) + drift / 3, 52 + drift,
                        laneY(2) + drift, TFT_DARKGREY);
        canvas.fillCircle(12 + drift / 2, laneY(1) - drift, 2, TFT_ORANGE);
      }
      return;
    }

    drawStationHealth(canvas, health_);
    if (stationImpactMs_ > 0) {
      drawExplosion(canvas, 27, laneY(stationImpactLane_),
                    280 - stationImpactMs_);
    }

    const int shipY = laneY(shipLane_);
    canvas.drawTriangle(PLAYER_X + 15, shipY, PLAYER_X - 10, shipY - 11,
                        PLAYER_X - 10, shipY + 11, TFT_WHITE);
    canvas.fillTriangle(PLAYER_X + 11, shipY, PLAYER_X - 7, shipY - 7,
                        PLAYER_X - 7, shipY + 7, TFT_CYAN);
    if (shieldHits_ > 0) canvas.drawCircle(PLAYER_X, shipY, 15, TFT_GREEN);
    if (shieldHits_ > 2) canvas.drawCircle(PLAYER_X, shipY, 19, TFT_GREEN);
    if (shieldHits_ > 5) canvas.drawCircle(PLAYER_X, shipY, 23, TFT_GREEN);
    for (uint8_t i = 0; i < ENEMIES; i++) {
      if (enemies_[i].active)
        drawEnemyShape(canvas, enemies_[i], laneY(0), laneY(1), laneY(2),
                       laneY(enemies_[i].lane));
    }
    for (uint8_t i = 0; i < SHOTS; i++) {
      if (shots_[i].active) {
        const int x = static_cast<int>(shots_[i].x);
        const int y = static_cast<int>(shots_[i].y);
        canvas.drawFastHLine(x - 2, y, 9, TFT_YELLOW);
        canvas.drawPixel(x + 8, y, TFT_WHITE);
      }
    }
    for (uint8_t i = 0; i < ENEMY_SHOTS; i++) {
      if (!enemyShots_[i].active) continue;
      const int x = static_cast<int>(enemyShots_[i].x);
      const int y = static_cast<int>(enemyShots_[i].y);
      canvas.drawLine(x - 7, y, x + 2, y, TFT_RED);
      canvas.drawPixel(x - 8, y, TFT_YELLOW);
    }
    for (uint8_t i = 0; i < POWERS; i++) {
      if (!powers_[i].active) continue;
      drawPowerShape(canvas, powers_[i], laneY(powers_[i].lane));
    }
  });
}

void AlienRaidersGame::drawEnemy(TFT_eSPI& tft, const Enemy& enemy) const {
  drawEnemyShape(tft, enemy, laneY(0), laneY(1), laneY(2),
                 laneY(enemy.lane));
}

void AlienRaidersGame::drawPower(TFT_eSPI& tft, const Power& power) const {
  drawPowerShape(tft, power, laneY(power.lane));
}

void AlienRaidersGame::drawIntro(TFT_eSPI& tft) const {
  drawIntroFrame(tft, introTimerMs_, laneY(0), laneY(1), laneY(2));
}

void AlienRaidersGame::drawStart(TFT_eSPI& tft) {
  loadBestScore();
  const bool promptPage = showStartPromptPage();
  const bool scorePage = showStartScorePage();
  CydFramebuffer::draw(tft, PORTRAIT_W, PORTRAIT_H, [&](auto& canvas) {
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextDatum(TL_DATUM);
    if (promptPage) {
      drawPortraitHeader(canvas, appTitle(), TFT_CYAN);
      drawCentered(canvas, "Press to Start", 78, 3, TFT_WHITE);
      drawPortraitFooter(canvas, "Tap Start");
    } else if (scorePage) {
      char initials[4];
      PlayerProfile::unpackDottedInitials(bestInitials_, initials);
      drawPortraitHeader(canvas, "Top Kills", TFT_YELLOW);
      drawCentered(canvas, "Best", 42, 2, TFT_LIGHTGREY);
      drawCentered(canvas, bestScore_ == 0 ? String("--") : String(bestScore_), 68, 4, TFT_YELLOW);
      drawCentered(canvas, bestScore_ == 0 ? String("") : String(initials), 116, 2, TFT_LIGHTGREY);
      drawPortraitFooter(canvas, "Best defense");
    } else {
      drawPortraitHeader(canvas, appTitle(), TFT_CYAN);
      drawSpacedock(canvas, 82, 31, 86, 108);

      // Tiny ships emphasize the scale of the dock.
      canvas.fillTriangle(169, 56, 157, 51, 157, 61, TFT_CYAN);
      canvas.drawTriangle(169, 56, 157, 51, 157, 61, TFT_WHITE);
      canvas.fillTriangle(214, 91, 202, 86, 202, 96, TFT_LIGHTGREY);
      canvas.fillTriangle(252, 46, 242, 42, 242, 50, TFT_RED);
      canvas.drawFastHLine(174, 56, 19, TFT_DARKGREY);
      canvas.drawFastHLine(219, 91, 12, TFT_DARKGREY);
      canvas.setTextSize(2);
      canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
      canvas.drawString("Defend", 184, 112);
      canvas.drawString("the station", 164, 130);
      drawPortraitFooter(canvas, "Tap top / bottom to change lane");
    }
  });
}

void AlienRaidersGame::drawEnd(TFT_eSPI& tft) {
  CydFramebuffer::draw(tft, PORTRAIT_W, PORTRAIT_H, [&](auto& canvas) {
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextDatum(TL_DATUM);
    drawPortraitHeader(canvas, "Defeated", TFT_RED);
    drawCentered(canvas, "Kills", 38, 1, TFT_LIGHTGREY);
    drawCentered(canvas, String(score_), 54, 3, TFT_CYAN);
    String best = String(bestScore_);
    String initialsLabel;
    if (bestScore_ > 0) {
      char initials[4];
      PlayerProfile::unpackDottedInitials(bestInitials_, initials);
      initialsLabel = initials;
    }
    drawCentered(canvas, "Best", 91, 1, TFT_LIGHTGREY);
    drawCentered(canvas, best, 106, 2, TFT_YELLOW);
    if (initialsLabel.length() > 0) {
      drawCentered(canvas, initialsLabel, 128, 2, TFT_LIGHTGREY);
    }
    drawPortraitFooter(canvas, "Tap Play Again");
  });
}

uint16_t AlienRaidersGame::staticRenderIntervalMs() const {
  // Start pages rotate every two seconds; the end page is completely static.
  return phase() == AppPhase::Start ? 2000 : 0;
}

int AlienRaidersGame::laneY(uint8_t lane) const {
  return PLAY_TOP + static_cast<int>(lane) * LANE_H + LANE_H / 2;
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
