#pragma once

#include "../../App.h"

class AlienRaidersGame : public App {
  public:
    AlienRaidersGame(uint32_t width, uint32_t height, uint32_t left);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

  private:
    static constexpr uint8_t LANES = 3;
    static constexpr uint8_t ENEMIES = 8;
    static constexpr uint8_t SHOTS = 12;
    static constexpr uint8_t ENEMY_SHOTS = 6;
    static constexpr uint8_t POWERS = 4;

    struct Enemy {
      float x = 0.0f;
      uint8_t lane = 0;
      uint8_t hp = 1;
      uint8_t maxHp = 1;
      bool boss = false;
      bool active = false;
    };

    struct Shot {
      float x = 0.0f;
      float y = 0.0f;
      int8_t lane = 0;
      int8_t dy = 0;
      bool active = false;
    };

    struct Power {
      float x = 0.0f;
      uint8_t lane = 0;
      uint8_t type = 0;
      bool active = false;
    };

    struct EnemyShot {
      float x = 0.0f;
      float y = 0.0f;
      int8_t dy = 0;
      bool active = false;
    };

    int laneY(uint8_t lane) const;
    void loadBestScore();
    void saveBestScore();
    void spawnEnemy();
    void spawnBoss();
    void fireShot(uint8_t lane, int8_t dy);
    void spawnPower(float x, uint8_t lane);
    void applyPower(uint8_t type);
    void smartBomb();
    void loseHealth();
    void fireBossShot(int8_t dy);
    void drawEnemy(U8G2& u8g2, const Enemy& enemy) const;
    void drawPower(U8G2& u8g2, const Power& power) const;
    void drawIntro(U8G2& u8g2) const;

    uint32_t left_;
    uint8_t shipLane_ = 1;
    uint8_t health_ = 5;
    uint8_t shieldHits_ = 0;
    uint16_t rapidMs_ = 0;
    uint16_t spreadMs_ = 0;
    uint16_t doubleMs_ = 0;
    uint16_t spawnTimerMs_ = 0;
    uint16_t shotTimerMs_ = 0;
    uint16_t bossShotTimerMs_ = 0;
    uint16_t introTimerMs_ = 0;
    uint16_t nextBossScore_ = 100;
    uint8_t weaponLevel_ = 0;
    uint16_t score_ = 0;
    uint16_t bestScore_ = 0;
    uint16_t bestInitials_ = 0;
    bool bestLoaded_ = false;
    bool introActive_ = false;
    bool bossActive_ = false;
    bool spreadShotToggle_ = false;
    Enemy enemies_[ENEMIES] = {};
    Shot shots_[SHOTS] = {};
    EnemyShot enemyShots_[ENEMY_SHOTS] = {};
    Power powers_[POWERS] = {};
};
