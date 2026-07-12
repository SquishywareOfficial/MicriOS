#pragma once

#include "../../App.h"
#include "../shared/logic/TongItsLogic.h"

class TongItsGame : public App {
  public:
    TongItsGame(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;
    uint16_t runningRenderIntervalMs() const override;
    uint16_t staticRenderIntervalMs() const override;

  protected:
    bool consumesButton2HoldInRunning() const override;
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;

  private:
    template <typename Canvas>
    void drawShell(Canvas& canvas, const char* subtitle);
    template <typename Canvas>
    void drawCard(Canvas& canvas, int x, int y, uint8_t card, bool selected = false, bool small = false);
    template <typename Canvas>
    void drawHand(Canvas& canvas);
    template <typename Canvas>
    void drawPiles(Canvas& canvas);
    template <typename Canvas>
    void drawMelds(Canvas& canvas);
    template <typename Canvas>
    void drawTable(Canvas& canvas);
    template <typename Canvas>
    void drawGameOver(Canvas& canvas);

    const char* viewName() const;
    const char* phaseName() const;
    const char* playerName(uint8_t player) const;

    TongIts::Logic logic_;
};
