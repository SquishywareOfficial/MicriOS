#pragma once

#include "../../App.h"

class FishingFlickGame : public App {
  public:
    FishingFlickGame(uint32_t width, uint32_t height, uint32_t left);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

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
    void drawWater(U8G2& u8g2);

    uint32_t left_;
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
