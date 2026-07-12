#pragma once

#include "../../App.h"
#include "../shared/logic/DiceRollerLogic.h"

class DiceRollerApp : public App {
  public:
    DiceRollerApp(uint32_t width, uint32_t height, uint32_t left = 1);

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    bool startsRunningImmediately() const override;
    bool hasCustomOverlay() const override;

  private:
    DiceRollerLogic logic_;
};
