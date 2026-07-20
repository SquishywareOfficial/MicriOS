#pragma once

#include "../../App.h"

class NoonShooterGame : public App {
  public:
    NoonShooterGame(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;

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
    void drawRevolver(TFT_eSPI& tft, bool playerShot, uint8_t frame);
    void drawGunslinger(TFT_eSPI& tft, int x, int y, bool facingRight);
    void drawFallenGunslinger(TFT_eSPI& tft, int x, int y, bool facingRight);

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
