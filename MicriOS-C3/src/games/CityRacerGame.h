#pragma once

#include "../../App.h"

class CityRacerGame : public App {
  public:
    CityRacerGame(uint32_t width, uint32_t height, uint32_t left);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

  private:
    static constexpr uint8_t LANES = 3;
    static constexpr uint8_t ROWS = 4;

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
    void drawCar(U8G2& u8g2, int x, int y, bool player) const;
    uint8_t chooseSafeLane();

    uint32_t left_;
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
