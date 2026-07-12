#pragma once

#include "../../App.h"

class TFT_eSprite;

class CityRacerGame : public App {
  public:
    CityRacerGame(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;
    uint16_t runningRenderIntervalMs() const override;

  private:
    static constexpr uint8_t LANES = 3;
    static constexpr uint8_t ROWS = 6;

    struct TrafficRow {
      float y = 0.0f;
      uint8_t mask = 0;
      bool counted = false;
      bool active = false;
    };

    int laneX(uint8_t lane) const;
    void loadBestScore();
    void saveBestScore();
    void spawnRow();
    void drawCar(TFT_eSPI& tft, int x, int y, bool player) const;
    void drawCar(TFT_eSprite& sprite, int x, int y, bool player) const;
    uint8_t chooseSafeLane();

    uint8_t playerLane_ = 1;
    uint8_t plannedSafeLane_ = 1;
    uint8_t rowsSinceLaneChange_ = 0;
    uint8_t level_ = 1;
    uint16_t spawnTimerMs_ = 0;
    uint16_t spawnIntervalMs_ = 1200;
    float speed_ = 30.0f;
    uint16_t score_ = 0;
    uint16_t bestScore_ = 0;
    uint16_t bestInitials_ = 0;
    bool bestLoaded_ = false;
    TrafficRow rows_[ROWS] = {};
};
