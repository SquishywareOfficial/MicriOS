#pragma once

#include "../../App.h"
#include "../shared/logic/MouseEmulatorLogic.h"

class MouseEmulatorApp : public App {
  public:
    MouseEmulatorApp(uint32_t width, uint32_t height);

  protected:
    bool updateStart(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    bool hasCustomOverlay() const override;
    void onAppExit() override;

  private:
    void ensureProfileLoaded();
    void cycleProfile();
    void sendHumanizedStep();
    template <typename Canvas> void drawDashboard(Canvas& tft, bool connected, bool enabled, int8_t connectionPhase, int32_t countdownValue);

    MouseEmulatorLogic logic_;
    uint32_t lastClockUpdate_ = 0;
    int stepsPerTick_ = 30;
    uint8_t profileIndex_ = 0;
    bool profileLoaded_ = false;

    // UI state tracking to minimize redraws
    bool uiInitialized_ = false;
    bool lastKnownConnection_ = false;
    bool lastKnownEnabled_ = false;
    int8_t lastConnectionPhase_ = -1;
    int32_t lastCountdownValue_ = INT32_MIN;
    int16_t lastMovementPixels_ = -1;
};
