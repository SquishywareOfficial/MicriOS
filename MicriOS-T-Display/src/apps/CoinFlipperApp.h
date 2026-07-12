#pragma once

#include "../../App.h"
#include "../shared/logic/CoinFlipperLogic.h"

class CoinFlipperApp : public App {
  public:
    CoinFlipperApp(uint32_t width, uint32_t height);

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    bool hasCustomOverlay() const override;
    bool startsRunningImmediately() const override;

  private:
    CoinFlipperLogic logic_;
    CoinFlipperLogic::Mode renderedMode_ = CoinFlipperLogic::Mode::Ready;
    bool renderedHeads_ = true;
    uint16_t renderedFrame_ = UINT16_MAX;
    bool dirty_ = true;
};
