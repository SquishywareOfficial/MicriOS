#pragma once

#include "../../App.h"

class BlackjackGame : public App {
  public:
    BlackjackGame(uint32_t width, uint32_t height, uint32_t left);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

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
    void drawCards(U8G2& u8g2, int x, int y, const uint8_t* cards, uint8_t count, bool hideFirst);

    uint32_t left_;
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
};
