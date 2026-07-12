#pragma once

#include "../../App.h"
#include "../shared/logic/CountdownLogic.h"

class CountdownApp : public App {
  public:
    CountdownApp(uint32_t width, uint32_t height);

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    bool hasCustomOverlay() const override;
    bool startsRunningImmediately() const override;

  private:
    void drawCountdownFrame(TFT_eSPI& tft, uint32_t remainingMs, bool running, uint16_t stateColor);
    void drawFinishedAlert(TFT_eSPI& tft, bool flashOn);

    CountdownLogic logic_;
    bool uiInitialized_ = false;
    bool lastRunning_ = false;
    bool alarmActive_ = false;
    bool lastAlarmFlash_ = false;
    uint32_t lastDurationMs_ = 0;
    uint32_t lastRenderedSecond_ = UINT32_MAX;
};
