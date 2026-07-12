#pragma once

#include "../../App.h"

class NuclearReactorGame : public App {
  public:
    NuclearReactorGame(uint32_t width, uint32_t height, uint32_t left);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

  private:
    void loadBest();
    void saveBest();

    uint32_t left_;
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
