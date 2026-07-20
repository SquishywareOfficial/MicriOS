#pragma once

#include "../../App.h"

class NuclearReactorGame : public App {
  public:
    NuclearReactorGame(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;

  private:
    void loadBest();
    void saveBest();

    float temp_ = 58.0f;
    uint16_t rodsMs_ = 0;
    float instability_ = 0.0f;
    float currentPowerKw_ = 0.0f;
    uint32_t scoreWh_ = 0;
    uint32_t bestWh_ = 0;
    uint16_t bestInitials_ = 0;
    uint16_t secondMs_ = 0;
    uint16_t survivedSec_ = 0;
    bool bestLoaded_ = false;
    bool meltdown_ = false;
    bool stalled_ = false;
};
