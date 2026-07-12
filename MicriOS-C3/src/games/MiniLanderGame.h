#pragma once

#include "../../App.h"

class MiniLanderGame : public App {
  public:
    MiniLanderGame(uint32_t width, uint32_t height, uint32_t left);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

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
    void drawSafeSpeedSign(U8G2& u8g2, int x, int y, int safeSpeed);
    void drawBriefing(U8G2& u8g2);
    void drawHud(U8G2& u8g2);
    void drawLander(U8G2& u8g2, int x, int y, bool thrusting);
    void drawGround(U8G2& u8g2);

    uint32_t left_;
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
