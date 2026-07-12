#pragma once

#include "../../App.h"

class SimonGame : public App {
  public:
    SimonGame(uint32_t width, uint32_t height, uint32_t left);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

  private:
    enum class Mode { Show, Input, Good };

    void loadBest();
    void saveBest();
    void addStep();
    void checkStep(bool longTone);

    uint32_t left_;
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
