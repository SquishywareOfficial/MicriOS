#pragma once

#include "../../App.h"

class MiniLanderGame : public App {
  public:
    MiniLanderGame(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;

  private:
    enum class LanderState {
      Briefing,
      Falling,
      Landed
    };

    void loadLevel();
    void loadBestLevel();
    void saveBestLevel();
    void recordLandedLevel();
    float levelGravity() const;
    float levelSafeSpeed() const;
    bool shouldShowBurnHint() const;
    void setBurnHintLed(bool on);
    void drawSafeSpeedSign(TFT_eSPI& tft, int x, int y, int safeSpeed);
    void drawBriefing(TFT_eSPI& tft);
    void drawHud(TFT_eSPI& tft);
    void drawLander(TFT_eSPI& tft, int x, int y, bool thrusting);
    void drawGround(TFT_eSPI& tft);

    LanderState landerState_ = LanderState::Falling;
    uint8_t level_ = 1;
    uint8_t bestLevel_ = 0;
    uint16_t bestInitials_ = 0;
    float altitude_ = 0.0f;
    float velocity_ = 0.0f;
    float fuel_ = 0.0f;
    float startAltitude_ = 0.0f;
    float startFuel_ = 0.0f;
    uint16_t briefingTimerMs_ = 0;
    uint16_t landedTimerMs_ = 0;
    bool briefingCanAcceptInput_ = false;
    bool thrusting_ = false;
    bool crashed_ = false;
    bool bestLoaded_ = false;
};
