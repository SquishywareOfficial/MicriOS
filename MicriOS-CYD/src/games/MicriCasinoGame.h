#pragma once

#include "../../App.h"
#include "../shared/logic/CasinoLogic.h"

class MicriCasinoGame : public App {
  public:
    MicriCasinoGame(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;
    uint16_t runningRenderIntervalMs() const override;
    uint16_t staticRenderIntervalMs() const override;
    bool handleTouch(const TouchUi::TouchSample& sample,
                     const TouchUi::TouchEvent& event) override;
    bool usesDefaultTouchPhaseControls() const override { return false; }
    bool consumeTongItsLaunchRequest();
    void resumeHubFromTongIts();

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
    void chooseHubItem(uint8_t index);
    void chooseBet(uint8_t index);
    const char* hubLabel(uint8_t index) const;
    uint16_t playerInitials() const;
    Casino::Money holdemRaiseChoice(uint8_t index) const;
    template <typename Canvas>
    void drawShell(Canvas& canvas, const char* title);
    template <typename Canvas>
    void drawCard(Canvas& canvas, int x, int y, uint8_t card, bool hidden = false);
    template <typename Canvas>
    void drawSuit(Canvas& canvas, int x, int y, uint8_t suit);
    template <typename Canvas>
    void drawHub(Canvas& canvas);
    template <typename Canvas>
    void drawHubScene(Canvas& canvas, uint8_t index);
    template <typename Canvas>
    void drawBlackjack(Canvas& canvas);
    template <typename Canvas>
    void drawRoulette(Canvas& canvas);
    template <typename Canvas>
    void drawSlots(Canvas& canvas);
    template <typename Canvas>
    void drawHoldem(Canvas& canvas);
    template <typename Canvas>
    void drawHoldemRaisePicker(Canvas& canvas);
    template <typename Canvas>
    void drawVideoPoker(Canvas& canvas);
    template <typename Canvas>
    void drawVideoPaytable(Canvas& canvas);
    template <typename Canvas>
    void drawBroke(Canvas& canvas);
    template <typename Canvas>
    void drawCashOut(Canvas& canvas);

    Casino::Logic logic_;
    bool loaded_ = false;
    Casino::Money savedBankroll_ = 0;
    Casino::Money savedBestCashout_ = 0;
    uint16_t savedBestCashoutInitials_ = 0;
    bool confirmReset_ = false;
    bool launchTongItsRequested_ = false;
    bool holdemRaisePicker_ = false;
    uint8_t holdemRaiseIndex_ = 0;
    bool videoPaytable_ = false;
    uint8_t hubIndex_ = 0;
    TouchUi::ControlCapture touchCapture_;
};
