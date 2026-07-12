#pragma once

#include "../../App.h"
#include "../shared/logic/MouseEmulatorLogic.h"

class MouseEmulatorApp : public App {
  public:
    MouseEmulatorApp(uint32_t width, uint32_t height, uint32_t left = 1);

  protected:
    bool updateStart(uint32_t deltaMs, const ButtonInput& input) override;
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    bool hasCustomOverlay() const override;
    void onAppExit() override;

  private:
    void ensureProfileLoaded();
    void cycleProfile();
    void sendHumanizedStep();

    MouseEmulatorLogic logic_;
    uint32_t lastClockUpdate_ = 0;
    int stepsPerTick_ = 30;
    uint8_t profileIndex_ = 0;
    bool profileLoaded_ = false;
};
