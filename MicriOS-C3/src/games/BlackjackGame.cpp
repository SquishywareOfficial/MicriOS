#include "BlackjackGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
constexpr uint16_t DEAL_DELAY_MS = 300;
constexpr uint16_t DEALER_DELAY_MS = 500;
constexpr uint16_t SHUFFLE_DELAY_MS = 1000;
Preferences blackjackPrefs;
}

BlackjackGame::BlackjackGame(uint32_t width, uint32_t height, uint32_t left)
    : App("Blackjack", width, height), left_(left) {}

bool BlackjackGame::hasCustomOverlay() const {
  return true;
}

void BlackjackGame::onAppReset() {
  loadBestBankroll();
  bankroll_ = 100;
  bet_ = 10;
  shuffleDeck();
  beginBetting();
}

void BlackjackGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  switch (handState_) {
    case HandState::Shuffling:
      actionTimerMs_ += deltaMs;
      if (actionTimerMs_ >= SHUFFLE_DELAY_MS) {
        shuffleDeck();
        beginBetting();
      }
      break;

    case HandState::Betting:
      if (bankroll_ <= 0) {
        endApp();
      } else if (input.click) {
        startDeal();
      } else if (input.longPress) {
        cycleBet();
      }
      break;

    case HandState::Dealing:
      updateDeal(deltaMs);
      break;

    case HandState::PlayerTurn:
      if (input.longPress) {
        stand();
      } else if (input.click) {
        addPlayerCard();
        if (handValue(playerCards_, playerCount_) > 21 || playerCount_ >= MAX_CARDS) {
          result_ = RoundResult::Bust;
          bankroll_ -= bet_;
          settleRound();
        }
      }
      break;

    case HandState::DealerReveal:
    case HandState::DealerTurn:
      updateDealer(deltaMs);
      break;

    case HandState::RoundOver:
      if (input.longPress || bankroll_ <= 0) {
        endApp();
      } else if (input.click) {
        if (deckRemaining_ <= 10) {
          handState_ = HandState::Shuffling;
          actionTimerMs_ = 0;
        } else {
          beginBetting();
        }
      }
      break;
  }
}

void BlackjackGame::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 2, 6);
  u8g2.print("$");
  u8g2.print(bankroll_);
  u8g2.setCursor(left_ + 38, 6);
  u8g2.print("Bet ");
  u8g2.print(bet_);

  if (handState_ == HandState::Shuffling) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(left_ + 12, 20, "Shuffle");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(left_ + 8, 32, "2 deck shoe");
    return;
  }

  if (handState_ == HandState::Betting) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(left_ + 17, 18, "Bet");
    u8g2.setCursor(left_ + 38, 18);
    u8g2.print(bet_);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(left_ + 2, 29);
    u8g2.print(deckRemaining_);
    u8g2.print(" cards");
    u8g2.drawStr(left_ + 2, 38, "Tap deal Hold bet");
    return;
  }

  const bool hideDealer = handState_ == HandState::Dealing || handState_ == HandState::PlayerTurn;
  u8g2.drawStr(left_ + 2, 15, "D");
  drawCards(u8g2, left_ + 10, 15, dealerCards_, dealerCount_, hideDealer);
  u8g2.drawStr(left_ + 2, 27, "P");
  drawCards(u8g2, left_ + 10, 27, playerCards_, playerCount_, false);

  if (handState_ == HandState::Dealing) {
    u8g2.drawStr(left_ + 2, 38, "Dealing");
  } else if (handState_ == HandState::PlayerTurn) {
    u8g2.drawStr(left_ + 2, 38, "Tap hit Hold stay");
  } else if (handState_ == HandState::DealerReveal) {
    u8g2.drawStr(left_ + 2, 38, "Dealer shows");
  } else if (handState_ == HandState::DealerTurn) {
    u8g2.drawStr(left_ + 2, 38, handValue(dealerCards_, dealerCount_) < 17 ? "Dealer hit" : "Dealer stay");
  } else {
    u8g2.drawStr(left_ + 2, 38, resultText());
  }
}

void BlackjackGame::drawStart(U8G2& u8g2) {
  loadBestBankroll();
  u8g2.drawFrame(0, 0, width + 2, height);
  if (showStartPromptPage()) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(20, 16, "Press");
    u8g2.drawStr(13, 29, "to Start");
    return;
  }
  if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 9, "Top Bank");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(3, 24);
    if (bestBankroll_ <= 100) {
      u8g2.print("--");
    } else {
      u8g2.print(initials);
      u8g2.print(" $");
      u8g2.print(bestBankroll_);
    }
    return;
  }

  u8g2.drawFrame(8, 16, 10, 14);
  u8g2.drawFrame(21, 14, 10, 14);
  u8g2.drawDisc(50, 24, 5);
  u8g2.drawCircle(58, 25, 5);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, appTitle());
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(14, 38, "2 deck shoe");
}

void BlackjackGame::drawEnd(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, "Cash Out");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(3, 20);
  u8g2.print("Bank ");
  u8g2.print(bankroll_);
  u8g2.setCursor(3, 29);
  u8g2.print("Best ");
  u8g2.print(bestBankroll_);
  if (bestBankroll_ > 100) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.print(" ");
    u8g2.print(initials);
  }
  u8g2.drawStr(3, 38, "Tap retry Hold menu");
}

void BlackjackGame::loadBestBankroll() {
  if (bestLoaded_) {
    return;
  }
  blackjackPrefs.begin("blackjack", true);
  bestBankroll_ = blackjackPrefs.getShort("best", 100);
  bestInitials_ = blackjackPrefs.getUShort("init", PlayerProfile::defaultInitials());
  blackjackPrefs.end();
  bestLoaded_ = true;
}

void BlackjackGame::saveBestBankroll() {
  bestInitials_ = PlayerProfile::loadInitials();
  blackjackPrefs.begin("blackjack", false);
  blackjackPrefs.putShort("best", bestBankroll_);
  blackjackPrefs.putUShort("init", bestInitials_);
  blackjackPrefs.end();
}

void BlackjackGame::beginBetting() {
  handState_ = HandState::Betting;
  result_ = RoundResult::None;
  playerCount_ = 0;
  dealerCount_ = 0;
  actionTimerMs_ = 0;
  dealStep_ = 0;
  if (bankroll_ < bet_) {
    bet_ = max<int16_t>(1, min<int16_t>(10, bankroll_));
  }
}

void BlackjackGame::startDeal() {
  if (bankroll_ <= 0) {
    endApp();
    return;
  }
  if (deckRemaining_ <= 10) {
    handState_ = HandState::Shuffling;
    actionTimerMs_ = 0;
    return;
  }
  playerCount_ = 0;
  dealerCount_ = 0;
  dealStep_ = 0;
  actionTimerMs_ = DEAL_DELAY_MS;
  result_ = RoundResult::None;
  handState_ = HandState::Dealing;
}

void BlackjackGame::updateDeal(uint32_t deltaMs) {
  actionTimerMs_ += deltaMs;
  if (actionTimerMs_ < DEAL_DELAY_MS) {
    return;
  }
  actionTimerMs_ = 0;
  if (dealStep_ == 0) {
    addPlayerCard();
  } else if (dealStep_ == 1) {
    addDealerCard();
  } else if (dealStep_ == 2) {
    addPlayerCard();
  } else if (dealStep_ == 3) {
    addDealerCard();
  }
  dealStep_++;
  if (dealStep_ >= 4) {
    const bool playerNatural = isNaturalBlackjack(playerCards_, playerCount_);
    const bool dealerNatural = isNaturalBlackjack(dealerCards_, dealerCount_);
    if (playerNatural || dealerNatural) {
      if (playerNatural && dealerNatural) {
        result_ = RoundResult::Push;
      } else if (playerNatural) {
        result_ = RoundResult::Blackjack;
        bankroll_ += (bet_ * 3) / 2;
      } else {
        result_ = RoundResult::Lose;
        bankroll_ -= bet_;
      }
      saveBestIfNeeded();
      handState_ = HandState::RoundOver;
    } else {
      handState_ = HandState::PlayerTurn;
    }
  }
}

void BlackjackGame::updateDealer(uint32_t deltaMs) {
  actionTimerMs_ += deltaMs;
  if (actionTimerMs_ < DEALER_DELAY_MS) {
    return;
  }
  actionTimerMs_ = 0;

  if (handState_ == HandState::DealerReveal) {
    handState_ = HandState::DealerTurn;
    return;
  }

  if (handValue(dealerCards_, dealerCount_) < 17 && dealerCount_ < MAX_CARDS) {
    addDealerCard();
    return;
  }
  settleRound();
}

void BlackjackGame::shuffleDeck() {
  for (uint8_t i = 0; i < DECK_SIZE; i++) {
    deck_[i] = (i % 13) + 1;
  }
  for (int i = DECK_SIZE - 1; i > 0; i--) {
    const int j = random(0, i + 1);
    const uint8_t tmp = deck_[i];
    deck_[i] = deck_[j];
    deck_[j] = tmp;
  }
  deckRemaining_ = DECK_SIZE;
}

uint8_t BlackjackGame::drawCardFromDeck() {
  if (deckRemaining_ == 0) {
    shuffleDeck();
  }
  return deck_[--deckRemaining_];
}

void BlackjackGame::addPlayerCard() {
  if (playerCount_ < MAX_CARDS) {
    playerCards_[playerCount_++] = drawCardFromDeck();
  }
}

void BlackjackGame::addDealerCard() {
  if (dealerCount_ < MAX_CARDS) {
    dealerCards_[dealerCount_++] = drawCardFromDeck();
  }
}

uint8_t BlackjackGame::handValue(const uint8_t* cards, uint8_t count) const {
  uint8_t total = 0;
  uint8_t aces = 0;
  for (uint8_t i = 0; i < count; i++) {
    const uint8_t card = cards[i];
    if (card == 1) {
      total += 11;
      aces++;
    } else if (card > 10) {
      total += 10;
    } else {
      total += card;
    }
  }
  while (total > 21 && aces > 0) {
    total -= 10;
    aces--;
  }
  return total;
}

bool BlackjackGame::isNaturalBlackjack(const uint8_t* cards, uint8_t count) const {
  return count == 2 && handValue(cards, count) == 21;
}

void BlackjackGame::cycleBet() {
  const int16_t options[] = {10, 20, 30, 50};
  for (uint8_t i = 0; i < 4; i++) {
    if (options[i] > bet_ && options[i] <= bankroll_) {
      bet_ = options[i];
      return;
    }
  }
  bet_ = bankroll_ >= 10 ? 10 : bankroll_;
}

void BlackjackGame::stand() {
  handState_ = HandState::DealerReveal;
  actionTimerMs_ = 0;
}

void BlackjackGame::settleRound() {
  if (result_ == RoundResult::None) {
    const uint8_t player = handValue(playerCards_, playerCount_);
    const uint8_t dealer = handValue(dealerCards_, dealerCount_);
    if (player > 21) {
      result_ = RoundResult::Bust;
      bankroll_ -= bet_;
    } else if (dealer > 21 || player > dealer) {
      result_ = isNaturalBlackjack(playerCards_, playerCount_) ? RoundResult::Blackjack : RoundResult::Win;
      bankroll_ += result_ == RoundResult::Blackjack ? (bet_ * 3) / 2 : bet_;
    } else if (player == dealer) {
      result_ = RoundResult::Push;
    } else {
      result_ = RoundResult::Lose;
      bankroll_ -= bet_;
    }
  }
  saveBestIfNeeded();
  handState_ = HandState::RoundOver;
}

void BlackjackGame::saveBestIfNeeded() {
  if (bankroll_ > bestBankroll_) {
    bestBankroll_ = bankroll_;
    saveBestBankroll();
  }
}

const char* BlackjackGame::cardLabel(uint8_t card) const {
  switch (card) {
    case 1:
      return "A";
    case 11:
      return "J";
    case 12:
      return "Q";
    case 13:
      return "K";
    case 10:
      return "T";
    case 9:
      return "9";
    case 8:
      return "8";
    case 7:
      return "7";
    case 6:
      return "6";
    case 5:
      return "5";
    case 4:
      return "4";
    case 3:
      return "3";
    default:
      return "2";
  }
}

const char* BlackjackGame::resultText() const {
  switch (result_) {
    case RoundResult::Win:
      return "Win Tap next";
    case RoundResult::Lose:
      return "Lose Tap next";
    case RoundResult::Push:
      return "Push Tap next";
    case RoundResult::Bust:
      return "Bust Tap next";
    case RoundResult::Blackjack:
      return "Blackjack 3:2";
    case RoundResult::None:
    default:
      return "";
  }
}

void BlackjackGame::drawCards(U8G2& u8g2, int x, int y, const uint8_t* cards, uint8_t count, bool hideFirst) {
  const uint8_t visibleCards = min<uint8_t>(count, 6);
  for (uint8_t i = 0; i < visibleCards; i++) {
    u8g2.drawFrame(x + i * 8, y - 6, 7, 8);
    u8g2.drawStr(x + i * 8 + 2, y, hideFirst && i == 0 ? "?" : cardLabel(cards[i]));
  }
  if (count > visibleCards) {
    u8g2.drawStr(x + visibleCards * 8 + 1, y, "+");
  }
  if (!hideFirst && count > 0) {
    u8g2.setCursor(x + 48, y);
    u8g2.print(handValue(cards, count));
  }
}
