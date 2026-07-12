#include "BlackjackGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
constexpr uint16_t DEAL_DELAY_MS = 300;
constexpr uint16_t DEALER_DELAY_MS = 500;
constexpr uint16_t SHUFFLE_DELAY_MS = 1000;
Preferences blackjackPrefs;
}

BlackjackGame::BlackjackGame(uint32_t width, uint32_t height)
    : App("Blackjack", width, height) {}

bool BlackjackGame::hasCustomOverlay() const {
  return true;
}

void BlackjackGame::onAppReset() {
  loadBestBankroll();
  bankroll_ = 100;
  bet_ = 10;
  runningDrawDirty_ = true;
  renderedHandState_ = 255;
  renderedResult_ = 255;
  renderedPlayerCount_ = 255;
  renderedDealerCount_ = 255;
  renderedDealStep_ = 255;
  renderedDeckRemaining_ = 255;
  renderedBankroll_ = -32768;
  renderedBet_ = -32768;
  shuffleDeck();
  beginBetting();
}

void BlackjackGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)b2;
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
      } else if (b1.click) {
        startDeal();
      } else if (b1.longPress) {
        cycleBet();
      }
      break;

    case HandState::Dealing:
      updateDeal(deltaMs);
      break;

    case HandState::PlayerTurn:
      if (b1.longPress) {
        stand();
      } else if (b1.click) {
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
      if (b1.longPress || bankroll_ <= 0) {
        endApp();
      } else if (b1.click) {
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

void BlackjackGame::drawRunning(TFT_eSPI& tft) {
  const uint8_t state = static_cast<uint8_t>(handState_);
  const uint8_t result = static_cast<uint8_t>(result_);
  const bool unchanged =
      !runningDrawDirty_ &&
      renderedHandState_ == state &&
      renderedResult_ == result &&
      renderedPlayerCount_ == playerCount_ &&
      renderedDealerCount_ == dealerCount_ &&
      renderedDealStep_ == dealStep_ &&
      renderedDeckRemaining_ == deckRemaining_ &&
      renderedBankroll_ == bankroll_ &&
      renderedBet_ == bet_;
  if (unchanged) {
    return;
  }

  runningDrawDirty_ = false;
  renderedHandState_ = state;
  renderedResult_ = result;
  renderedPlayerCount_ = playerCount_;
  renderedDealerCount_ = dealerCount_;
  renderedDealStep_ = dealStep_;
  renderedDeckRemaining_ = deckRemaining_;
  renderedBankroll_ = bankroll_;
  renderedBet_ = bet_;

  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    const String bank = "$" + String(bankroll_) + " B" + String(bet_);
    TDisplayUi::header(canvas, "Blackjack", TFT_GREEN, bank.c_str());

    if (handState_ == HandState::Shuffling) {
      canvas.fillRoundRect(79, 44, 34, 46, 4, TFT_NAVY);
      canvas.drawRoundRect(79, 44, 34, 46, 4, TFT_WHITE);
      canvas.fillRoundRect(127, 44, 34, 46, 4, TFT_DARKGREEN);
      canvas.drawRoundRect(127, 44, 34, 46, 4, TFT_WHITE);
      TDisplayUi::centered(canvas, "Shuffling", 96, 2, TFT_YELLOW);
      TDisplayUi::footer(canvas, "Two-deck shoe");
      return;
    }

    if (handState_ == HandState::Betting) {
      canvas.setTextSize(2);
      canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      canvas.drawString("Bet", 35, 48);
      TDisplayUi::largeValue(canvas, "$" + String(bet_), 63, TFT_YELLOW);
      canvas.setTextSize(1);
      canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      canvas.drawString(String(deckRemaining_) + " cards left", 86, 105);
      TDisplayUi::footer(canvas, "B1 tap deal  Hold bet");
      return;
    }

    const bool hideDealer = handState_ == HandState::Dealing || handState_ == HandState::PlayerTurn;
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas.drawString("Dealer", 10, 36);
    drawCards(canvas, 82, 35, dealerCards_, dealerCount_, hideDealer);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.drawString("Player", 10, 75);
    drawCards(canvas, 82, 74, playerCards_, playerCount_, false);

    if (handState_ == HandState::Dealing) {
      TDisplayUi::footer(canvas, "Dealing...");
    } else if (handState_ == HandState::PlayerTurn) {
      TDisplayUi::footer(canvas, "B1 hit  Hold stay");
    } else if (handState_ == HandState::DealerReveal) {
      TDisplayUi::footer(canvas, "Dealer reveals");
    } else if (handState_ == HandState::DealerTurn) {
      TDisplayUi::footer(canvas, handValue(dealerCards_, dealerCount_) < 17 ? "Dealer hits" : "Dealer stays");
    } else {
      TDisplayUi::footer(canvas, resultText());
    }
  });
}

void BlackjackGame::drawStart(TFT_eSPI& tft) {
  TDisplayUi::clear(tft);
  loadBestBankroll();
  if (showStartPromptPage()) {
    TDisplayUi::header(tft, "Blackjack", TFT_GREEN);
    TDisplayUi::centered(tft, "Press", 50, 3, TFT_WHITE);
    TDisplayUi::centered(tft, "to Start", 78, 2, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "B1 start");
    return;
  }
  if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);

    TDisplayUi::header(tft, "Top Bank", TFT_YELLOW);
    TDisplayUi::largeValue(tft, bestBankroll_ <= 100 ? String("--") : String("$") + String(bestBankroll_), 52, TFT_YELLOW);
    TDisplayUi::centered(tft, bestBankroll_ <= 100 ? String("") : String(initials), 95, 2, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "Best bankroll");
    return;
  }

  TDisplayUi::header(tft, appTitle(), TFT_GREEN);
  tft.fillRoundRect(42, 52, 32, 45, 4, TFT_WHITE);
  tft.drawRoundRect(42, 52, 32, 45, 4, TFT_DARKGREY);
  tft.setTextSize(2);
  tft.setTextColor(TFT_RED, TFT_WHITE);
  tft.drawString("A", 52, 64);
  tft.fillRoundRect(80, 44, 32, 45, 4, TFT_WHITE);
  tft.drawRoundRect(80, 44, 32, 45, 4, TFT_DARKGREY);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString("K", 90, 56);
  tft.fillCircle(157, 75, 18, TFT_RED);
  tft.drawCircle(157, 75, 18, TFT_YELLOW);
  tft.fillCircle(185, 78, 18, TFT_BLUE);
  tft.drawCircle(185, 78, 18, TFT_WHITE);
  TDisplayUi::footer(tft, "Two-deck shoe");
}

void BlackjackGame::drawEnd(TFT_eSPI& tft) {
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Cash Out", TFT_GREEN);

  TDisplayUi::labelValue(tft, 48, "Bank", "$" + String(bankroll_), bankroll_ > 100 ? TFT_GREEN : TFT_YELLOW);
  String best = bestBankroll_ > 100 ? "$" + String(bestBankroll_) : String("--");
  if (bestBankroll_ > 100) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    best += " ";
    best += initials;
  }
  TDisplayUi::labelValue(tft, 78, "Best", best, TFT_YELLOW);
  TDisplayUi::footer(tft, "B1 retry  Hold menu");
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

template <typename Canvas>
void BlackjackGame::drawCards(Canvas& tft, int x, int y, const uint8_t* cards, uint8_t count, bool hideFirst) {
  const uint8_t visibleCards = min<uint8_t>(count, 6);
  for (uint8_t i = 0; i < visibleCards; i++) {
    const int cardX = x + i * 24;
    if (hideFirst && i == 0) {
      tft.fillRoundRect(cardX, y, 20, 28, 3, TFT_NAVY);
      tft.drawRoundRect(cardX, y, 20, 28, 3, TFT_WHITE);
      tft.drawLine(cardX + 4, y + 6, cardX + 16, y + 22, TFT_CYAN);
      tft.drawLine(cardX + 16, y + 6, cardX + 4, y + 22, TFT_CYAN);
    } else {
      tft.fillRoundRect(cardX, y, 20, 28, 3, TFT_WHITE);
      tft.drawRoundRect(cardX, y, 20, 28, 3, TFT_DARKGREY);
      const bool red = cards[i] == 1 || cards[i] > 10;
      tft.setTextSize(2);
      tft.setTextColor(red ? TFT_RED : TFT_BLACK, TFT_WHITE);
      tft.drawString(cardLabel(cards[i]), cardX + 5, y + 7);
    }
  }
  if (count > visibleCards) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("+", x + visibleCards * 24 + 2, y + 8);
  }
  if (!hideFirst && count > 0) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(String(handValue(cards, count)), 215, y + 7);
  }
}
