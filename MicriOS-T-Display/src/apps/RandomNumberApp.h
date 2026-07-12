#pragma once

#include "../../App.h"
#include "../shared/logic/RandomNumberLogic.h"

class RandomNumberApp : public App {
  public:
    RandomNumberApp(uint32_t width, uint32_t height);
    void render(TFT_eSPI& tft) override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    bool hasCustomOverlay() const override;
    bool startsRunningImmediately() const override;

  private:
    void markDirty();

    RandomNumberLogic logic_;
    bool dirty_ = true;
    bool startDirty_ = true;
    bool phaseCached_ = false;
    AppPhase renderedPhase_ = AppPhase::Start;
};
