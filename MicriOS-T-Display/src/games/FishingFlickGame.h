#pragma once

#include "../../App.h"

class FishingFlickGame : public App {
  public:
    FishingFlickGame(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;

  private:
    enum class FishState {
      Waiting,
      Hooking,
      Reeling,
      Caught,
      Broken,
      Escaped
    };

    void loadBestFish();
    void saveBestFish();
    void loadLevel();
    float difficulty() const;
    void startFightBurst();
    void setWarningLed(bool on);
    void updateWarningLed();
    void drawWater(TFT_eSPI& tft);

    FishState fishState_ = FishState::Waiting;
    uint16_t waitMs_ = 0;
    uint16_t elapsedMs_ = 0;
    uint16_t hookTimerMs_ = 0;
    uint16_t fightTimerMs_ = 0;
    uint16_t caughtTimerMs_ = 0;
    uint16_t caughtWeight_ = 0;
    uint16_t fishWeight_ = 0;
    uint16_t bestWeight_ = 0;
    uint8_t level_ = 1;
    uint16_t bestInitials_ = 0;
    float progress_ = 0.0f;
    float tension_ = 0.0f;
    bool fighting_ = false;
    bool bestLoaded_ = false;
};
