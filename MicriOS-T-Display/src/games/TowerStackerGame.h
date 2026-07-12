#pragma once

#include "../../App.h"

class TowerStackerGame : public App {
  public:
    TowerStackerGame(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;

  private:
    static constexpr uint8_t MAX_LAYERS = 18;
    static constexpr uint8_t LEVEL_HEIGHT = 10;
    static constexpr int BLOCK_H = 8;
    static constexpr int HUD_H = 30;

    struct Layer {
      int x = 0;
      int w = 0;
    };

    void loadHighScore();
    void saveHighScore();
    void startTowerLevel();
    void dropMovingBlock();
    void prepareNextBlock(int blockWidth);
    int layerY(uint8_t layer, uint8_t firstVisibleLayer) const;

    Layer layers_[MAX_LAYERS] = {};
    uint8_t layerCount_ = 0;
    uint8_t level_ = 1;
    uint8_t towerHeight_ = 0;
    uint16_t score_ = 0;
    uint16_t highScore_ = 0;
    uint16_t highScoreInitials_ = 0;
    bool highScoreLoaded_ = false;
    float movingX_ = 0.0f;
    float movingSpeed_ = 24.0f;
    int movingDir_ = 1;
    int movingW_ = 0;
};
