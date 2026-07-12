#pragma once

#include "../../App.h"
#include "../shared/logic/MetronomeLogic.h"

class MetronomeApp : public App {
  public:
    MetronomeApp(uint32_t width, uint32_t height);

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    bool hasCustomOverlay() const override;
    bool startsRunningImmediately() const override;

  private:
    void drawMetronomeFrame(TFT_eSPI& tft, bool running, bool pulse);

    MetronomeLogic logic_;
    uint32_t pulseUntilMs_ = 0;
    bool uiInitialized_ = false;
    bool lastRunning_ = false;
    bool lastPulse_ = false;
    uint16_t lastBpm_ = 0;
};
