#pragma once

#include "../../App.h"

class NeedSpeedGame : public App {
  public:
    NeedSpeedGame(uint32_t width, uint32_t height, uint32_t left);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

  private:
    enum class RaceState {
      Countdown,
      LaunchWait,
      Racing,
      LevelComplete
    };

    enum class ShiftMessage {
      None,
      Short,
      Good,
      Perfect,
      Late,
      Limiter,
      FalseStart
    };

    void launch(uint32_t lateMs);
    void startLevel();
    void shift();
    void updateShiftLed(uint32_t nowMs);
    void setShiftLed(bool on);
    void loadBestRun();
    void saveBestRun();
    void recordClearedLevel(uint8_t clearedLevel);
    float levelFinishSpeed() const;
    uint16_t levelTargetMs() const;
    float levelRpmMultiplier() const;
    float levelSpeedMultiplier() const;
    void drawTrafficLights(U8G2& u8g2);
    void drawCarSplash(U8G2& u8g2);
    void drawTach(U8G2& u8g2);
    const char* messageText() const;

    uint32_t left_;
    RaceState raceState_ = RaceState::Countdown;
    ShiftMessage message_ = ShiftMessage::None;
    uint16_t countdownMs_ = 3000;
    uint32_t raceMs_ = 0;
    uint32_t totalRaceMs_ = 0;
    uint16_t messageTimerMs_ = 0;
    uint16_t levelCompleteTimerMs_ = 0;
    uint8_t level_ = 1;
    uint8_t gear_ = 1;
    uint8_t bestLevel_ = 0;
    uint16_t bestInitials_ = 0;
    uint32_t bestRaceMs_ = 0;
    float rpm_ = 900.0f;
    float speed_ = 0.0f;
    bool disqualified_ = false;
    bool failed_ = false;
    bool bestLoaded_ = false;
};
