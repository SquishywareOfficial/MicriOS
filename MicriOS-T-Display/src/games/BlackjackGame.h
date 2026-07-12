#pragma once

#include "../../App.h"

class BlackjackGame : public App {
  public:
    BlackjackGame(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;

  private:
    static constexpr uint8_t MAX_CARDS = 7;
    static constexpr uint8_t DECK_SIZE = 104;

    enum class HandState {
      Shuffling,
      Betting,
      Dealing,
      PlayerTurn,
      DealerReveal,
      DealerTurn,
      RoundOver
    };

    enum class RoundResult {
      None,
      Win,
      Lose,
      Push,
      Bust,
      Blackjack
    };

    void loadBestBankroll();
    void saveBestBankroll();
    void beginBetting();
    void startDeal();
    void updateDeal(uint32_t deltaMs);
    void updateDealer(uint32_t deltaMs);
    void shuffleDeck();
    uint8_t drawCardFromDeck();
    void addPlayerCard();
    void addDealerCard();
    uint8_t handValue(const uint8_t* cards, uint8_t count) const;
    bool isNaturalBlackjack(const uint8_t* cards, uint8_t count) const;
    void cycleBet();
    void stand();
    void settleRound();
    void saveBestIfNeeded();
    const char* cardLabel(uint8_t card) const;
    const char* resultText() const;
    template <typename Canvas>
    void drawCards(Canvas& tft, int x, int y, const uint8_t* cards, uint8_t count, bool hideFirst);

    HandState handState_ = HandState::Betting;
    RoundResult result_ = RoundResult::None;
    uint8_t deck_[DECK_SIZE] = {};
    uint8_t deckRemaining_ = 0;
    uint8_t playerCards_[MAX_CARDS] = {};
    uint8_t dealerCards_[MAX_CARDS] = {};
    uint8_t playerCount_ = 0;
    uint8_t dealerCount_ = 0;
    int16_t bankroll_ = 100;
    int16_t bestBankroll_ = 0;
    uint16_t bestInitials_ = 0;
    int16_t bet_ = 10;
    uint16_t actionTimerMs_ = 0;
    uint8_t dealStep_ = 0;
    bool bestLoaded_ = false;
    bool runningDrawDirty_ = true;
    uint8_t renderedHandState_ = 255;
    uint8_t renderedResult_ = 255;
    uint8_t renderedPlayerCount_ = 255;
    uint8_t renderedDealerCount_ = 255;
    uint8_t renderedDealStep_ = 255;
    uint8_t renderedDeckRemaining_ = 255;
    int16_t renderedBankroll_ = -32768;
    int16_t renderedBet_ = -32768;
};
