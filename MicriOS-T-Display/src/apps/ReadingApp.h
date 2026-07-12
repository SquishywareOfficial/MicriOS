#pragma once

#include "../../App.h"

class ReadingApp : public App {
  public:
    ReadingApp(uint32_t width, uint32_t height);

    bool hasCustomOverlay() const override;
    void render(TFT_eSPI& tft) override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    bool startsRunningImmediately() const override;

  private:
    enum class Mode {
      Select,
      Read,
      Complete
    };

    template <typename Canvas> void drawSelection(Canvas& tft) const;
    template <typename Canvas> void drawReading(Canvas& tft);
    template <typename Canvas> void drawComplete(Canvas& tft) const;
    template <typename Canvas> void drawCenteredText(Canvas& tft, int y, const char* text) const;
    void markDirty();

    uint8_t selection_;
    uint8_t page_;
    Mode mode_;
    bool pageHasMore_ = false;
    bool dirty_ = true;
    bool startDirty_ = true;
    bool phaseCached_ = false;
    AppPhase renderedPhase_ = AppPhase::Start;
};
