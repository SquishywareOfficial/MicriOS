#pragma once

#include "../../App.h"
#include "../shared/logic/DiceRollerLogic.h"

class DiceRollerApp : public App {
  public:
    DiceRollerApp(uint32_t width, uint32_t height);

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    bool hasCustomOverlay() const override;
    bool startsRunningImmediately() const override;

  private:
    DiceRollerLogic logic_;
    DiceRollerLogic::Mode renderedMode_ = DiceRollerLogic::Mode::Select;
    uint8_t renderedDie_ = 0;
    uint8_t renderedResult_ = 0;
    uint16_t renderedRollFrame_ = UINT16_MAX;
    bool dirty_ = true;
};
