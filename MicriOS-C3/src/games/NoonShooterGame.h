#pragma once

#include "../../App.h"

class NoonShooterGame : public App {
  public:
    NoonShooterGame(uint32_t width, uint32_t height, uint32_t left);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

  private:
    enum class DuelState {
      Waiting,
      Draw,
      PlayerShot,
      EnemyShot
    };

    void loadScores();
    void saveBestReaction();
    void saveBestLevel();
    void startRound();
    uint16_t enemyDelayMs() const;
    void drawRevolver(U8G2& u8g2, bool playerShot, uint8_t frame);
    void drawGunslinger(U8G2& u8g2, int x, int y, bool facingRight);
    void drawFallenGunslinger(U8G2& u8g2, int x, int y, bool facingRight);

    uint32_t left_;
    DuelState duelState_ = DuelState::Waiting;
    uint8_t level_ = 1;
    uint8_t bestLevel_ = 0;
    uint16_t waitMs_ = 0;
    uint16_t elapsedMs_ = 0;
    uint16_t reactionMs_ = 0;
    uint16_t animMs_ = 0;
    uint16_t bestReactionMs_ = 0;
    uint16_t bestInitials_ = 0;
    bool bestLoaded_ = false;
    bool tooEarly_ = false;
    bool playerLost_ = false;
};
