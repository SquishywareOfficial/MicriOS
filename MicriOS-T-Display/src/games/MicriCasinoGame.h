#pragma once

#include "../../App.h"
#include "../shared/logic/CasinoLogic.h"

class MicriCasinoGame : public App {
  public:
    MicriCasinoGame(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;
    uint16_t runningRenderIntervalMs() const override;
    uint16_t staticRenderIntervalMs() const override;

  protected:
    void onAppReset() override;
    void onAppExit() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;

  private:
    void ensureLoaded();
    void saveBankroll();
    uint16_t playerInitials() const;
    template <typename Canvas>
    void drawShell(Canvas& canvas, const char* title);
    template <typename Canvas>
    void drawCard(Canvas& canvas, int x, int y, uint8_t card, bool hidden = false);
    template <typename Canvas>
    void drawSuit(Canvas& canvas, int x, int y, uint8_t suit);
    template <typename Canvas>
    void drawHub(Canvas& canvas);
    template <typename Canvas>
    void drawBlackjack(Canvas& canvas);
    template <typename Canvas>
    void drawRoulette(Canvas& canvas);
    template <typename Canvas>
    void drawSlots(Canvas& canvas);
    template <typename Canvas>
    void drawHoldem(Canvas& canvas);
    template <typename Canvas>
    void drawVideoPoker(Canvas& canvas);
    template <typename Canvas>
    void drawBroke(Canvas& canvas);
    template <typename Canvas>
    void drawCashOut(Canvas& canvas);

    Casino::Logic logic_;
    bool loaded_ = false;
    Casino::Money savedBankroll_ = 0;
    Casino::Money savedBestBankroll_ = 0;
    uint16_t savedBestInitials_ = 0;
};
