#pragma once

#include "../../App.h"
#include "../shared/logic/CasinoLogic.h"

class MicriCasinoGame : public App {
  public:
    MicriCasinoGame(uint32_t width, uint32_t height, uint32_t left);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void onAppExit() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

  private:
    void ensureLoaded();
    void saveBankroll();
    uint16_t playerInitials() const;
    void drawFrame(U8G2& u8g2, const char* title);
    void drawMoney(U8G2& u8g2, int x, int y, const char* label, Casino::Money value);
    void drawCardLine(U8G2& u8g2, int x, int y, const uint8_t* cards, uint8_t count, uint8_t visibleCount = 7);
    void drawBlackjackCards(U8G2& u8g2, int x, int y, const uint8_t* cards, uint8_t count, bool hideFirst);
    void drawHub(U8G2& u8g2);
    void drawBlackjack(U8G2& u8g2);
    void drawRoulette(U8G2& u8g2);
    void drawSlots(U8G2& u8g2);
    void drawHoldem(U8G2& u8g2);
    void drawVideoPoker(U8G2& u8g2);
    void drawBroke(U8G2& u8g2);
    void drawCashOut(U8G2& u8g2);

    uint32_t left_;
    Casino::Logic logic_;
    bool loaded_ = false;
    Casino::Money savedBankroll_ = 0;
    Casino::Money savedBestBankroll_ = 0;
    uint16_t savedBestInitials_ = 0;
};
