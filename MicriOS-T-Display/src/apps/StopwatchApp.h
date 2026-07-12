#pragma once

#include "../../App.h"
#include "../shared/logic/StopwatchLogic.h"

class StopwatchApp : public App {
  public:
    StopwatchApp(uint32_t width, uint32_t height);

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    bool hasCustomOverlay() const override;
    bool startsRunningImmediately() const override;

  private:
    void drawStatic(TFT_eSPI& tft, bool running);
    void drawElapsed(TFT_eSPI& tft, uint32_t elapsedMs, bool running);

    StopwatchLogic logic_;
    bool uiInitialized_ = false;
    bool lastRunning_ = false;
    uint32_t lastRenderedTick_ = UINT32_MAX;
};
