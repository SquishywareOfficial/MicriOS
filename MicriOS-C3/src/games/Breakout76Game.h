#pragma once

#include "../../App.h"

class Breakout76Game : public App {
  public:
    Breakout76Game(uint32_t width, uint32_t height, uint32_t left);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

  private:
    static constexpr uint8_t ROWS = 5;
    static constexpr uint8_t COLS = 9;
    static constexpr uint8_t BALLS = 3;
    static constexpr uint8_t POWERUPS = 3;

    struct Ball {
      float x = 0.0f;
      float y = 0.0f;
      float vx = 0.0f;
      float vy = 0.0f;
      bool active = false;
    };

    struct Power {
      float x = 0.0f;
      float y = 0.0f;
      uint8_t type = 0;
      bool active = false;
    };

    void loadBestScore();
    void saveBestScore();
    void buildLevel();
    void launchBall(uint8_t slot, bool alternate);
    void spawnPower(float x, float y);
    void applyPower(uint8_t type);
    uint8_t activeBallCount() const;
    int clampInt(int value, int minValue, int maxValue) const;

    uint32_t left_;
    float paddleX_ = 24.0f;
    float paddleVel_ = 0.0f;
    int8_t paddleDir_ = 1;
    uint8_t paddleW_ = 14;
    uint16_t wideTimerMs_ = 0;
    uint8_t level_ = 1;
    uint16_t score_ = 0;
    uint16_t bestScore_ = 0;
    uint16_t bestInitials_ = 0;
    bool bestLoaded_ = false;
    bool bricks_[ROWS][COLS] = {};
    uint8_t bricksLeft_ = 0;
    Ball balls_[BALLS] = {};
    Power powers_[POWERUPS] = {};
};
