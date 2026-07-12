#pragma once

#include "../../App.h"

class CreditsApp : public App {
  public:
    CreditsApp(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;
    void render(TFT_eSPI& tft) override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;

  private:
    enum class Mode {
      Select,
      License,
      Credits
    };

    void drawSelect(TFT_eSPI& tft);
    void drawLicense(TFT_eSPI& tft);
    void drawCredits(TFT_eSPI& tft);
    void markDirty();

    Mode mode_ = Mode::Select;
    uint8_t selection_ = 0;
    uint8_t page_ = 0;
    bool dirty_ = true;
    bool startDirty_ = true;
    bool endDirty_ = true;
    bool phaseCached_ = false;
    AppPhase renderedPhase_ = AppPhase::Start;
};
