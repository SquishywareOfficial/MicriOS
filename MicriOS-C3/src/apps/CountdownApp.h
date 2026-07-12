#pragma once

#include "../../App.h"
#include "../shared/logic/CountdownLogic.h"

class CountdownApp : public App {
  public:
    CountdownApp(uint32_t width, uint32_t height, uint32_t left = 1);

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    bool hasCustomOverlay() const override;

  private:
    CountdownLogic logic_;
};
