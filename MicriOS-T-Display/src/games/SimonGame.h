#pragma once

#include "../../App.h"

class SimonGame : public App {
  public:
    SimonGame(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;

  private:
    enum class Mode { Show, Input, Good };

    void loadBest();
    void saveBest();
    void addStep();
    void checkStep(bool longTone);

    bool sequence_[32] = {};
    uint8_t length_ = 0;
    uint8_t showIndex_ = 0;
    uint8_t inputIndex_ = 0;
    uint16_t timerMs_ = 0;
    Mode mode_ = Mode::Show;
    uint8_t bestLevel_ = 0;
    uint16_t bestInitials_ = 0;
    bool bestLoaded_ = false;
    bool flashLong_ = false;
    bool expectedLong_ = false;
    bool pressHandled_ = false;
};
