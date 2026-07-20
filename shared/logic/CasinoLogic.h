#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace Casino {

using Money = uint32_t;

constexpr Money START_BANKROLL = 200;
constexpr Money MAX_BANKROLL = 9999999UL;
constexpr uint8_t BET_COUNT = 4;

inline Money betAt(uint8_t index) {
  switch (index % BET_COUNT) {
    case 0: return 5;
    case 1: return 10;
    case 2: return 20;
    default: return 50;
  }
}

inline Money slotBetAt(uint8_t index) {
  switch (index % BET_COUNT) {
    case 0: return 1;
    case 1: return 2;
    case 2: return 5;
    default: return 10;
  }
}

enum class GameId : uint8_t {
  Blackjack,
  Roulette,
  Slots,
  Holdem,
  VideoPoker
};

enum class Screen : uint8_t {
  Hub,
  BlackjackShuffling,
  BlackjackBet,
  BlackjackDealing,
  BlackjackTurn,
  BlackjackDealerReveal,
  BlackjackDealerTurn,
  BlackjackResult,
  RouletteBet,
  RouletteStake,
  RouletteSpin,
  RouletteResult,
  SlotsIdle,
  SlotsSpinning,
  SlotsResult,
  HoldemBet,
  HoldemAction,
  HoldemResult,
  VideoBet,
  VideoHold,
  VideoResult,
  CashOut,
  Broke
};

enum class Outcome : uint8_t {
  None,
  Win,
  Lose,
  Push,
  Bust,
  Blackjack
};

enum class RouletteBet : uint8_t {
  Red,
  Black,
  Odd,
  Even,
  Low,
  High,
  Zero
};

enum class HoldemAction : uint8_t {
  Check,
  Raise,
  Fold
};

inline const char* gameName(GameId game) {
  switch (game) {
    case GameId::Blackjack: return "Blackjack";
    case GameId::Roulette: return "Roulette";
    case GameId::Slots: return "Slots";
    case GameId::Holdem: return "Holdem";
    case GameId::VideoPoker: return "Video Poker";
  }
  return "Casino";
}

inline const char* outcomeName(Outcome outcome) {
  switch (outcome) {
    case Outcome::Win: return "WIN";
    case Outcome::Lose: return "LOSE";
    case Outcome::Push: return "PUSH";
    case Outcome::Bust: return "BUST";
    case Outcome::Blackjack: return "BLACKJACK";
    case Outcome::None:
    default: return "";
  }
}

inline const char* blackjackResultName(Outcome outcome) {
  switch (outcome) {
    case Outcome::Win:
    case Outcome::Blackjack: return "WIN";
    case Outcome::Lose:
    case Outcome::Bust: return "LOSE";
    case Outcome::Push: return "DRAW";
    case Outcome::None:
    default: return "";
  }
}

inline const char* rouletteBetName(RouletteBet bet) {
  switch (bet) {
    case RouletteBet::Red: return "Red";
    case RouletteBet::Black: return "Black";
    case RouletteBet::Odd: return "Odd";
    case RouletteBet::Even: return "Even";
    case RouletteBet::Low: return "1-18";
    case RouletteBet::High: return "19-36";
    case RouletteBet::Zero: return "Zero";
  }
  return "Bet";
}

inline const char* holdemActionName(HoldemAction action) {
  switch (action) {
    case HoldemAction::Check: return "Check";
    case HoldemAction::Raise: return "Raise";
    case HoldemAction::Fold: return "Fold";
  }
  return "Act";
}

inline uint8_t cardRank(uint8_t card) {
  return static_cast<uint8_t>(card % 13);
}

inline uint8_t cardSuit(uint8_t card) {
  return static_cast<uint8_t>((card / 13) % 4);
}

inline uint8_t rankValue(uint8_t card) {
  const uint8_t rank = cardRank(card);
  return rank == 0 ? 14 : static_cast<uint8_t>(rank + 1);
}

inline char rankChar(uint8_t card) {
  static const char ranks[] = {'A', '2', '3', '4', '5', '6', '7', '8', '9', 'T', 'J', 'Q', 'K'};
  return ranks[cardRank(card)];
}

inline char suitChar(uint8_t card) {
  static const char suits[] = {'S', 'H', 'D', 'C'};
  return suits[cardSuit(card)];
}

inline void cardLabel(uint8_t card, char* out, uint8_t outSize) {
  if (outSize < 3) {
    return;
  }
  out[0] = rankChar(card);
  out[1] = suitChar(card);
  out[2] = '\0';
}

inline uint8_t blackjackValue(const uint8_t* cards, uint8_t count) {
  uint8_t total = 0;
  uint8_t aces = 0;
  for (uint8_t i = 0; i < count; i++) {
    const uint8_t rank = cardRank(cards[i]);
    if (rank == 0) {
      total += 11;
      aces++;
    } else if (rank >= 9) {
      total += 10;
    } else {
      total += rank + 1;
    }
  }
  while (total > 21 && aces > 0) {
    total -= 10;
    aces--;
  }
  return total;
}

inline bool isNaturalBlackjack(const uint8_t* cards, uint8_t count) {
  return count == 2 && blackjackValue(cards, count) == 21;
}

inline uint32_t packScore(uint8_t category, uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, uint8_t d = 0, uint8_t e = 0) {
  return (static_cast<uint32_t>(category) << 24) |
         (static_cast<uint32_t>(a) << 20) |
         (static_cast<uint32_t>(b) << 16) |
         (static_cast<uint32_t>(c) << 12) |
         (static_cast<uint32_t>(d) << 8) |
         (static_cast<uint32_t>(e) << 4);
}

inline uint8_t straightHigh(const bool* hasRank) {
  if (hasRank[14] && hasRank[5] && hasRank[4] && hasRank[3] && hasRank[2]) {
    return 5;
  }
  for (int8_t high = 14; high >= 6; high--) {
    bool straight = true;
    for (uint8_t offset = 0; offset < 5; offset++) {
      if (!hasRank[high - offset]) {
        straight = false;
        break;
      }
    }
    if (straight) {
      return static_cast<uint8_t>(high);
    }
  }
  return 0;
}

inline uint32_t evaluateFive(const uint8_t* cards) {
  uint8_t counts[15] = {};
  bool hasRank[15] = {};
  bool flush = true;
  const uint8_t suit = cardSuit(cards[0]);
  for (uint8_t i = 0; i < 5; i++) {
    const uint8_t rank = rankValue(cards[i]);
    counts[rank]++;
    hasRank[rank] = true;
    if (cardSuit(cards[i]) != suit) {
      flush = false;
    }
  }

  const uint8_t straight = straightHigh(hasRank);
  if (straight && flush) {
    return packScore(straight == 14 ? 10 : 9, straight);
  }

  uint8_t quad = 0;
  uint8_t trips = 0;
  uint8_t pair1 = 0;
  uint8_t pair2 = 0;
  for (int8_t r = 14; r >= 2; r--) {
    if (counts[r] == 4) quad = static_cast<uint8_t>(r);
    else if (counts[r] == 3 && trips == 0) trips = static_cast<uint8_t>(r);
    else if (counts[r] == 2) {
      if (pair1 == 0) pair1 = static_cast<uint8_t>(r);
      else if (pair2 == 0) pair2 = static_cast<uint8_t>(r);
    }
  }

  if (quad) {
    uint8_t kicker = 0;
    for (int8_t r = 14; r >= 2; r--) {
      if (counts[r] == 1) {
        kicker = static_cast<uint8_t>(r);
        break;
      }
    }
    return packScore(8, quad, kicker);
  }

  if (trips && pair1) {
    return packScore(7, trips, pair1);
  }

  if (flush) {
    uint8_t ranks[5] = {};
    uint8_t index = 0;
    for (int8_t r = 14; r >= 2 && index < 5; r--) {
      for (uint8_t n = 0; n < counts[r] && index < 5; n++) {
        ranks[index++] = static_cast<uint8_t>(r);
      }
    }
    return packScore(6, ranks[0], ranks[1], ranks[2], ranks[3], ranks[4]);
  }

  if (straight) {
    return packScore(5, straight);
  }

  if (trips) {
    uint8_t kickers[2] = {};
    uint8_t index = 0;
    for (int8_t r = 14; r >= 2 && index < 2; r--) {
      if (counts[r] == 1) {
        kickers[index++] = static_cast<uint8_t>(r);
      }
    }
    return packScore(4, trips, kickers[0], kickers[1]);
  }

  if (pair1 && pair2) {
    uint8_t kicker = 0;
    for (int8_t r = 14; r >= 2; r--) {
      if (counts[r] == 1) {
        kicker = static_cast<uint8_t>(r);
        break;
      }
    }
    return packScore(3, pair1, pair2, kicker);
  }

  if (pair1) {
    uint8_t kickers[3] = {};
    uint8_t index = 0;
    for (int8_t r = 14; r >= 2 && index < 3; r--) {
      if (counts[r] == 1) {
        kickers[index++] = static_cast<uint8_t>(r);
      }
    }
    return packScore(2, pair1, kickers[0], kickers[1], kickers[2]);
  }

  uint8_t ranks[5] = {};
  uint8_t index = 0;
  for (int8_t r = 14; r >= 2 && index < 5; r--) {
    if (counts[r] == 1) {
      ranks[index++] = static_cast<uint8_t>(r);
    }
  }
  return packScore(1, ranks[0], ranks[1], ranks[2], ranks[3], ranks[4]);
}

inline uint32_t evaluateBest(const uint8_t* cards, uint8_t count) {
  if (count < 5) {
    return 0;
  }
  uint32_t best = 0;
  uint8_t pick[5] = {};
  for (uint8_t a = 0; a < count - 4; a++) {
    for (uint8_t b = a + 1; b < count - 3; b++) {
      for (uint8_t c = b + 1; c < count - 2; c++) {
        for (uint8_t d = c + 1; d < count - 1; d++) {
          for (uint8_t e = d + 1; e < count; e++) {
            pick[0] = cards[a];
            pick[1] = cards[b];
            pick[2] = cards[c];
            pick[3] = cards[d];
            pick[4] = cards[e];
            const uint32_t score = evaluateFive(pick);
            if (score > best) {
              best = score;
            }
          }
        }
      }
    }
  }
  return best;
}

inline uint8_t scoreCategory(uint32_t score) {
  return static_cast<uint8_t>(score >> 24);
}

inline const char* pokerScoreName(uint32_t score) {
  switch (scoreCategory(score)) {
    case 10: return "Royal Flush";
    case 9: return "Straight Flush";
    case 8: return "Four Kind";
    case 7: return "Full House";
    case 6: return "Flush";
    case 5: return "Straight";
    case 4: return "Three Kind";
    case 3: return "Two Pair";
    case 2: return "Pair";
    default: return "High Card";
  }
}

inline uint16_t videoPokerMultiplier(uint32_t score) {
  const uint8_t category = scoreCategory(score);
  const uint8_t top = static_cast<uint8_t>((score >> 20) & 0x0F);
  switch (category) {
    case 10: return 250;
    case 9: return 50;
    case 8: return 25;
    case 7: return 9;
    case 6: return 6;
    case 5: return 4;
    case 4: return 3;
    case 3: return 2;
    case 2: return top >= 11 ? 1 : 0;
    default: return 0;
  }
}

class Logic {
  public:
    static constexpr uint16_t BLACKJACK_DEAL_DELAY_MS = 300;
    static constexpr uint16_t BLACKJACK_DEALER_DELAY_MS = 500;
    static constexpr uint16_t BLACKJACK_SHUFFLE_DELAY_MS = 1000;
    static constexpr uint8_t HOLDEM_SEATS = 4;

    void seed(uint32_t seedValue) {
      rng_ = seedValue == 0 ? 0xA53C9E11UL : seedValue;
    }

    void loadBankroll(Money bankroll, Money bestCashout, uint16_t initials) {
      bankroll_ = bankroll <= 0 ? START_BANKROLL : bankroll;
      bestCashout_ = bestCashout;
      if (bankroll_ > MAX_BANKROLL) bankroll_ = MAX_BANKROLL;
      if (bestCashout_ > MAX_BANKROLL) bestCashout_ = MAX_BANKROLL;
      bestCashoutInitials_ = initials;
      gameStartBankroll_ = bankroll_;
      clampBet();
    }

    Money bankroll() const { return bankroll_; }
    Money bestCashout() const { return bestCashout_; }
    uint16_t bestCashoutInitials() const { return bestCashoutInitials_; }
    Money lastCashout() const { return lastCashout_; }
    int32_t gameProfitLoss() const {
      return static_cast<int32_t>(bankroll_) - static_cast<int32_t>(gameStartBankroll_);
    }
    Money bet() const { return betAt(betIndex_); }
    Money slotBet() const { return slotBetAt(betIndex_); }
    uint8_t betIndex() const { return betIndex_; }
    bool setBetIndex(uint8_t index) {
      index = static_cast<uint8_t>(index % BET_COUNT);
      const Money wager = screen_ == Screen::SlotsIdle
                              ? slotBetAt(index)
                              : betAt(index);
      const Money budget = screen_ == Screen::RouletteStake
                               ? rouletteStakeBudget()
                               : bankroll_;
      if (wager > budget) return false;
      betIndex_ = index;
      dirty_ = true;
      return true;
    }
    Screen screen() const { return screen_; }
    uint8_t hubIndex() const { return hubIndex_; }
    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }
    GameId activeGame() const { return activeGame_; }

    uint8_t hubCount() const { return 7; }
    const char* hubLabel(uint8_t index) const {
      if (index < 5) {
        return gameName(static_cast<GameId>(index));
      }
      if (index == 5) {
        return "Cash Out";
      }
      return "Reset";
    }

    void resetToHub() {
      screen_ = Screen::Hub;
      outcome_ = Outcome::None;
      dirty_ = true;
    }

    void next(bool modernSlots) {
      switch (screen_) {
        case Screen::Hub:
          hubIndex_ = static_cast<uint8_t>((hubIndex_ + 1) % hubCount());
          break;
        case Screen::BlackjackShuffling:
        case Screen::BlackjackBet:
        case Screen::BlackjackDealing:
        case Screen::BlackjackTurn:
        case Screen::BlackjackDealerReveal:
        case Screen::BlackjackDealerTurn:
        case Screen::BlackjackResult:
          blackjackClick();
          break;
        case Screen::SlotsIdle:
        case Screen::RouletteStake:
        case Screen::HoldemBet:
        case Screen::VideoBet:
          cycleBet();
          break;
        case Screen::SlotsSpinning:
          break;
        case Screen::RouletteBet:
          rouletteBet_ = static_cast<RouletteBet>((static_cast<uint8_t>(rouletteBet_) + 1) % 7);
          break;
        case Screen::HoldemAction:
          holdemAction_ = static_cast<HoldemAction>((static_cast<uint8_t>(holdemAction_) + 1) % 3);
          break;
        case Screen::VideoHold:
          if (!videoAnimating_) {
            videoCursor_ = static_cast<uint8_t>((videoCursor_ + 1) % 6);
          }
          break;
        case Screen::RouletteResult:
          memset(rouletteBets_, 0, sizeof(rouletteBets_));
          screen_ = Screen::RouletteBet;
          break;
        case Screen::SlotsResult:
          screen_ = Screen::SlotsIdle;
          break;
        case Screen::HoldemResult:
          beginHoldemHand(currentInitials_);
          break;
        case Screen::VideoResult:
          dealVideoPoker(currentInitials_);
          break;
        case Screen::Broke:
          resetBankroll();
          break;
        case Screen::RouletteSpin:
          break;
        case Screen::CashOut:
          resetToHub();
          break;
      }
      dirty_ = true;
    }

    void previous() {
      if (screen_ == Screen::Hub) {
        hubIndex_ = hubIndex_ == 0 ? static_cast<uint8_t>(hubCount() - 1) : static_cast<uint8_t>(hubIndex_ - 1);
      } else {
        resetToHub();
      }
      dirty_ = true;
    }

    void select(bool modernSlots, uint16_t playerInitials) {
      switch (screen_) {
        case Screen::Hub:
          if (playerInitials != 0) {
            currentInitials_ = playerInitials;
          }
          if (hubIndex_ < 5) {
            activeGame_ = static_cast<GameId>(hubIndex_);
            gameStartBankroll_ = bankroll_;
            if (activeGame_ == GameId::Blackjack) startBlackjack();
            else if (activeGame_ == GameId::Roulette) startRoulette();
            else if (activeGame_ == GameId::Slots) screen_ = Screen::SlotsIdle;
            else if (activeGame_ == GameId::Holdem) startHoldem();
            else startVideoPoker();
          } else if (hubIndex_ == 5) {
            cashOut(currentInitials_);
          } else {
            resetBankroll();
          }
          break;
        case Screen::BlackjackShuffling:
        case Screen::BlackjackBet:
        case Screen::BlackjackDealing:
        case Screen::BlackjackTurn:
        case Screen::BlackjackDealerReveal:
        case Screen::BlackjackDealerTurn:
        case Screen::BlackjackResult:
          blackjackHold(playerInitials);
          break;
        case Screen::RouletteBet:
          screen_ = Screen::RouletteStake;
          break;
        case Screen::RouletteStake:
          commitRouletteBet(true, playerInitials);
          break;
        case Screen::SlotsIdle:
          startSlotsSpin(modernSlots);
          break;
        case Screen::HoldemBet:
          dealHoldemHand(playerInitials);
          break;
        case Screen::HoldemAction:
          playHoldemAction(playerInitials);
          break;
        case Screen::VideoBet:
          dealVideoPoker(playerInitials);
          break;
        case Screen::VideoHold:
          if (videoAnimating_) {
            break;
          }
          if (videoCursor_ < 5) {
            videoHeld_[videoCursor_] = !videoHeld_[videoCursor_];
          } else {
            drawVideoPoker(playerInitials);
          }
          break;
        case Screen::Broke:
          resetBankroll();
          break;
        case Screen::RouletteSpin:
        case Screen::SlotsSpinning:
          break;
        case Screen::CashOut:
          resetToHub();
          break;
        case Screen::VideoResult:
          startVideoPoker();
          break;
        default:
          resetToHub();
          break;
      }
      dirty_ = true;
    }

    void tick(uint32_t deltaMs, uint16_t playerInitials) {
      if (playerInitials != 0) {
        currentInitials_ = playerInitials;
      }
      if (screen_ == Screen::BlackjackShuffling) {
        actionTimerMs_ += deltaMs;
        if (actionTimerMs_ >= BLACKJACK_SHUFFLE_DELAY_MS) {
          shuffleBlackjackDeck();
          beginBlackjackBetting();
        }
        dirty_ = true;
      } else if (screen_ == Screen::BlackjackDealing) {
        updateBlackjackDeal(deltaMs);
      } else if (screen_ == Screen::BlackjackDealerReveal || screen_ == Screen::BlackjackDealerTurn) {
        updateBlackjackDealer(deltaMs);
      } else if (screen_ == Screen::RouletteSpin) {
        updateRouletteSpin(deltaMs);
      } else if (screen_ == Screen::SlotsSpinning) {
        updateSlotsSpin(deltaMs);
      } else if (screen_ == Screen::VideoHold && videoAnimating_) {
        updateVideoAnimation(deltaMs, playerInitials);
      } else if (screen_ == Screen::HoldemAction) {
        updateHoldem(deltaMs, playerInitials);
      }
    }

    void cashOut(uint16_t playerInitials) {
      lastCashout_ = bankroll_;
      if (lastCashout_ > bestCashout_) {
        bestCashout_ = lastCashout_;
        bestCashoutInitials_ = playerInitials;
      }
      bankroll_ = START_BANKROLL;
      betIndex_ = 0;
      screen_ = Screen::CashOut;
      dirty_ = true;
    }

    void resetBankroll() {
      bankroll_ = START_BANKROLL;
      betIndex_ = 0;
      gameStartBankroll_ = bankroll_;
      screen_ = Screen::Hub;
      dirty_ = true;
    }

    const uint8_t* playerCards() const { return playerCards_; }
    const uint8_t* dealerCards() const { return dealerCards_; }
    uint8_t playerCount() const { return playerCount_; }
    uint8_t dealerCount() const { return dealerCount_; }
    uint8_t blackjackPlayerValue() const { return blackjackValue(playerCards_, playerCount_); }
    uint8_t blackjackDealerValue() const { return blackjackValue(dealerCards_, dealerCount_); }
    uint8_t blackjackAction() const { return 0; }
    uint8_t blackjackDeckRemaining() const { return blackjackDeckRemaining_; }
    bool blackjackHideDealer() const { return screen_ == Screen::BlackjackDealing || screen_ == Screen::BlackjackTurn; }
    const char* blackjackFooterText() const {
      switch (screen_) {
        case Screen::BlackjackShuffling: return "Shuffling";
        case Screen::BlackjackBet: return "Tap deal Hold bet";
        case Screen::BlackjackDealing: return "Dealing";
        case Screen::BlackjackTurn: return "Tap hit Hold stay";
        case Screen::BlackjackDealerReveal: return "Dealer shows";
        case Screen::BlackjackDealerTurn: return blackjackDealerValue() < 17 ? "Dealer hit" : "Dealer stay";
        case Screen::BlackjackResult: return blackjackResultText();
        default: return "";
      }
    }
    const char* blackjackResultText() const {
      switch (outcome_) {
        case Outcome::Win: return "Win Tap next";
        case Outcome::Lose: return "Lose Tap next";
        case Outcome::Push: return "Push Tap next";
        case Outcome::Bust: return "Bust Tap next";
        case Outcome::Blackjack: return "Blackjack 3:2";
        case Outcome::None:
        default: return "";
      }
    }
    Outcome outcome() const { return outcome_; }
    Money lastWin() const { return lastWin_; }

    RouletteBet rouletteBet() const { return rouletteBet_; }
    Money rouletteWager(RouletteBet bet) const {
      return rouletteBets_[static_cast<uint8_t>(bet) % 7];
    }
    Money rouletteTotalWager() const {
      Money total = 0;
      for (uint8_t i = 0; i < 7; ++i) total += rouletteBets_[i];
      return total;
    }
    Money rouletteStakeBudget() const {
      const Money selected = rouletteWager(rouletteBet_);
      const Money committedElsewhere = rouletteTotalWager() - selected;
      return bankroll_ > committedElsewhere ? bankroll_ - committedElsewhere : 0;
    }
    bool commitRouletteBet(bool spin, uint16_t playerInitials) {
      if (bet() > rouletteStakeBudget()) return false;
      rouletteBets_[static_cast<uint8_t>(rouletteBet_)] = bet();
      if (spin) {
        startRouletteSpin(playerInitials);
      } else {
        screen_ = Screen::RouletteBet;
        dirty_ = true;
      }
      return true;
    }
    uint8_t rouletteNumber() const { return rouletteNumber_; }
    uint16_t rouletteSpinMs() const { return rouletteSpinMs_; }
    uint8_t rouletteDisplayNumber() const {
      if (screen_ != Screen::RouletteSpin) {
        return rouletteNumber_;
      }
      return static_cast<uint8_t>((rouletteNumber_ + 37 - ((rouletteSpinMs_ / 45) % 37)) % 37);
    }
    bool rouletteWon() const { return rouletteWon_; }

    uint8_t slotSymbol(uint8_t col, uint8_t row) const {
      return slots_[col % 5][row % 3];
    }
    uint8_t slotDisplaySymbol(uint8_t col, uint8_t row) const {
      if (screen_ != Screen::SlotsSpinning) {
        return slotSymbol(col, row);
      }
      const uint16_t stopAt = static_cast<uint16_t>(550 + col * 150);
      if (slotSpinMs_ >= stopAt) {
        return slotSymbol(col, row);
      }
      return static_cast<uint8_t>((slotSymbol(col, row) + (slotSpinMs_ / 55) + col * 3 + row * 2) % 8);
    }
    uint16_t slotSpinMs() const { return slotSpinMs_; }
    bool slotsSpinning() const { return screen_ == Screen::SlotsSpinning; }
    uint8_t slotCols() const { return slotCols_; }
    uint8_t freeSpins() const { return freeSpins_; }
    uint8_t scatters() const { return slotScatters_; }
    bool slotWinCell(uint8_t col, uint8_t row) const {
      const uint8_t bit = static_cast<uint8_t>((row % 3) * 5 + (col % 5));
      return (slotWinMask_ & (1U << bit)) != 0;
    }

    const uint8_t* holdemPlayer() const { return holdemCards_[0]; }
    const uint8_t* holdemDealer() const { return holdemCards_[1]; }
    const uint8_t* holdemSeatCards(uint8_t seat) const { return holdemCards_[seat % HOLDEM_SEATS]; }
    const uint8_t* holdemCommunity() const { return holdemCommunity_; }
    uint8_t communityShown() const { return holdemCommunityShown_; }
    HoldemAction holdemAction() const { return holdemAction_; }
    void setHoldemAction(HoldemAction action) {
      holdemAction_ = action;
      dirty_ = true;
    }
    Money pot() const { return holdemPot_; }
    uint32_t playerPokerScore() const { return playerPokerScore_; }
    uint32_t dealerPokerScore() const { return dealerPokerScore_; }
    uint32_t holdemWinningScore() const { return holdemWinningScore_; }
    uint32_t holdemSeatScore(uint8_t seat) const { return holdemSeatScores_[seat % HOLDEM_SEATS]; }
    bool holdemSeatFolded(uint8_t seat) const { return holdemFolded_[seat % HOLDEM_SEATS]; }
    bool holdemSeatAllIn(uint8_t seat) const { return holdemAllIn_[seat % HOLDEM_SEATS]; }
    uint8_t holdemActor() const { return holdemActor_; }
    uint8_t holdemWinnerSeat() const { return holdemWinnerSeat_; }
    uint8_t holdemWinnerCount() const { return holdemWinnerCount_; }
    bool holdemPlayerFolded() const { return holdemFolded_[0]; }
    Money holdemBigBlind() const { return holdemBigBlind_; }
    Money holdemMaxBet() const { return holdemMaxBet_; }
    Money holdemCallAmount() const {
      return holdemCurrentBet_ > holdemStreetBet_[0]
                 ? holdemCurrentBet_ - holdemStreetBet_[0]
                 : 0;
    }
    Money holdemRaiseAmount() const { return holdemRaiseAmount_; }
    bool holdemCanRaise() const {
      return holdemCurrentBet_ < holdemMaxBet_ && bankroll_ > holdemCallAmount();
    }
    void setHoldemRaiseAmount(Money amount) {
      const Money minimum = holdemBigBlind_ == 0 ? bet() : holdemBigBlind_;
      const Money room = holdemCurrentBet_ < holdemMaxBet_
                             ? holdemMaxBet_ - holdemCurrentBet_
                             : 0;
      if (amount < minimum) amount = minimum;
      if (amount > room) amount = room;
      holdemRaiseAmount_ = amount;
      dirty_ = true;
    }
    const char* holdemMessage() const { return holdemMessage_; }
    const char* holdemSeatName(uint8_t seat) const {
      static const char* const names[HOLDEM_SEATS] = {"You", "Blaze", "Sage", "Ace"};
      return names[seat % HOLDEM_SEATS];
    }
    const char* holdemPrimaryActionName() const {
      return holdemCallAmount() == 0 ? "Check" : "Call";
    }

    const uint8_t* videoCards() const { return videoCards_; }
    uint8_t videoDisplayCard(uint8_t index) const {
      index %= 5;
      if (!videoAnimating_ || (videoDrawPhase_ && videoHeld_[index])) {
        return videoCards_[index];
      }
      const uint16_t settleAt = static_cast<uint16_t>(300 + index * 90);
      if (videoAnimationMs_ >= settleAt) {
        return videoCards_[index];
      }
      return static_cast<uint8_t>((videoCards_[index] + videoAnimationMs_ / 45 + index * 7) % 52);
    }
    bool videoHeld(uint8_t index) const { return videoHeld_[index % 5]; }
    uint8_t videoCursor() const { return videoCursor_; }
    uint16_t videoMultiplier() const { return videoMultiplier_; }
    uint32_t videoScore() const { return videoScore_; }
    bool videoAnimating() const { return videoAnimating_; }
    bool videoDrawing() const { return videoAnimating_ && videoDrawPhase_; }

  private:
    uint32_t rand32() {
      rng_ ^= rng_ << 13;
      rng_ ^= rng_ >> 17;
      rng_ ^= rng_ << 5;
      return rng_;
    }

    void buildDeck() {
      for (uint8_t i = 0; i < 52; i++) {
        deck_[i] = i;
      }
      deckRemaining_ = 52;
      for (uint8_t i = 0; i < 52; i++) {
        const uint8_t j = static_cast<uint8_t>(rand32() % 52);
        const uint8_t tmp = deck_[i];
        deck_[i] = deck_[j];
        deck_[j] = tmp;
      }
    }

    void shuffleBlackjackDeck() {
      for (uint8_t i = 0; i < 104; i++) {
        blackjackDeck_[i] = static_cast<uint8_t>(i % 52);
      }
      for (int i = 103; i > 0; i--) {
        const int j = static_cast<int>(rand32() % static_cast<uint32_t>(i + 1));
        const uint8_t tmp = blackjackDeck_[i];
        blackjackDeck_[i] = blackjackDeck_[j];
        blackjackDeck_[j] = tmp;
      }
      blackjackDeckRemaining_ = 104;
    }

    uint8_t drawBlackjackCard() {
      if (blackjackDeckRemaining_ == 0) {
        shuffleBlackjackDeck();
      }
      return blackjackDeck_[--blackjackDeckRemaining_];
    }

    uint8_t drawCard() {
      if (deckRemaining_ == 0) {
        buildDeck();
      }
      return deck_[--deckRemaining_];
    }

    void cycleBet() {
      betIndex_ = static_cast<uint8_t>((betIndex_ + 1) % BET_COUNT);
      clampBet();
    }

    void clampBet() {
      while (bet() > bankroll_ && betIndex_ > 0) {
        betIndex_--;
      }
    }

    bool takeBet() {
      return takeWager(bet());
    }

    bool takeWager(Money wager) {
      if (bankroll_ < wager) {
        screen_ = Screen::Broke;
        return false;
      }
      bankroll_ -= wager;
      lastWin_ = 0;
      return true;
    }

    void loseBet() {
      bankroll_ = bankroll_ > bet() ? bankroll_ - bet() : 0;
      lastWin_ = 0;
      clampBet();
    }

    void addMoney(Money amount) {
      if (amount > MAX_BANKROLL - bankroll_) {
        bankroll_ = MAX_BANKROLL;
      } else {
        bankroll_ += amount;
      }
    }

    void pay(Money amount, uint16_t playerInitials) {
      (void)playerInitials;
      if (amount > 0) {
        addMoney(amount);
        lastWin_ = amount;
      } else {
        lastWin_ = 0;
      }
      clampBet();
    }

    void startBlackjack() {
      activeGame_ = GameId::Blackjack;
      if (blackjackDeckRemaining_ <= 10) {
        screen_ = Screen::BlackjackShuffling;
        actionTimerMs_ = 0;
      } else {
        beginBlackjackBetting();
      }
    }

    void beginBlackjackBetting() {
      screen_ = Screen::BlackjackBet;
      outcome_ = Outcome::None;
      playerCount_ = 0;
      dealerCount_ = 0;
      actionTimerMs_ = 0;
      dealStep_ = 0;
      clampBet();
      dirty_ = true;
    }

    void startBlackjackDeal() {
      if (bankroll_ <= 0) {
        screen_ = Screen::Broke;
        return;
      }
      if (blackjackDeckRemaining_ <= 10) {
        screen_ = Screen::BlackjackShuffling;
        actionTimerMs_ = 0;
        return;
      }
      playerCount_ = 0;
      dealerCount_ = 0;
      dealStep_ = 0;
      actionTimerMs_ = BLACKJACK_DEAL_DELAY_MS;
      outcome_ = Outcome::None;
      lastWin_ = 0;
      screen_ = Screen::BlackjackDealing;
      dirty_ = true;
    }

    void updateBlackjackDeal(uint32_t deltaMs) {
      actionTimerMs_ += deltaMs;
      if (actionTimerMs_ < BLACKJACK_DEAL_DELAY_MS) {
        return;
      }
      actionTimerMs_ = 0;
      if (dealStep_ == 0) {
        playerCards_[playerCount_++] = drawBlackjackCard();
      } else if (dealStep_ == 1) {
        dealerCards_[dealerCount_++] = drawBlackjackCard();
      } else if (dealStep_ == 2) {
        playerCards_[playerCount_++] = drawBlackjackCard();
      } else if (dealStep_ == 3) {
        dealerCards_[dealerCount_++] = drawBlackjackCard();
      }
      dealStep_++;
      if (dealStep_ >= 4) {
        const bool playerNatural = isNaturalBlackjack(playerCards_, playerCount_);
        const bool dealerNatural = isNaturalBlackjack(dealerCards_, dealerCount_);
        if (playerNatural || dealerNatural) {
          if (playerNatural && dealerNatural) {
            outcome_ = Outcome::Push;
            lastWin_ = 0;
          } else if (playerNatural) {
            outcome_ = Outcome::Blackjack;
            addMoney((bet() * 3) / 2);
            lastWin_ = (bet() * 3) / 2;
          } else {
            outcome_ = Outcome::Lose;
            loseBet();
          }
          clampBet();
          screen_ = Screen::BlackjackResult;
        } else {
          screen_ = Screen::BlackjackTurn;
        }
      }
      dirty_ = true;
    }

    void blackjackClick() {
      switch (screen_) {
        case Screen::BlackjackBet:
          startBlackjackDeal();
          break;
        case Screen::BlackjackTurn:
          blackjackHit();
          break;
        case Screen::BlackjackResult:
          if (bankroll_ <= 0) {
            screen_ = Screen::Broke;
          } else if (blackjackDeckRemaining_ <= 10) {
            screen_ = Screen::BlackjackShuffling;
            actionTimerMs_ = 0;
          } else {
            beginBlackjackBetting();
          }
          break;
        default:
          break;
      }
    }

    void blackjackHold(uint16_t playerInitials) {
      if (playerInitials != 0) {
        currentInitials_ = playerInitials;
      }
      switch (screen_) {
        case Screen::BlackjackBet:
          cycleBet();
          break;
        case Screen::BlackjackTurn:
          blackjackStand();
          break;
        case Screen::BlackjackResult:
          resetToHub();
          break;
        default:
          break;
      }
    }

    void blackjackHit() {
      if (playerCount_ < 7) {
        playerCards_[playerCount_++] = drawBlackjackCard();
      }
      if (blackjackValue(playerCards_, playerCount_) > 21 || playerCount_ >= 7) {
        outcome_ = Outcome::Bust;
        loseBet();
        screen_ = Screen::BlackjackResult;
      }
      dirty_ = true;
    }

    void blackjackStand() {
      screen_ = Screen::BlackjackDealerReveal;
      actionTimerMs_ = 0;
      dirty_ = true;
    }

    void updateBlackjackDealer(uint32_t deltaMs) {
      actionTimerMs_ += deltaMs;
      if (actionTimerMs_ < BLACKJACK_DEALER_DELAY_MS) {
        return;
      }
      actionTimerMs_ = 0;
      if (screen_ == Screen::BlackjackDealerReveal) {
        screen_ = Screen::BlackjackDealerTurn;
        dirty_ = true;
        return;
      }
      if (blackjackValue(dealerCards_, dealerCount_) < 17 && dealerCount_ < 7) {
        dealerCards_[dealerCount_++] = drawBlackjackCard();
        dirty_ = true;
        return;
      }
      settleBlackjack(currentInitials_);
    }

    void settleBlackjack(uint16_t playerInitials) {
      const uint8_t player = blackjackValue(playerCards_, playerCount_);
      const uint8_t dealer = blackjackValue(dealerCards_, dealerCount_);
      if (player > 21) {
        outcome_ = Outcome::Bust;
        loseBet();
      } else if (dealer > 21 || player > dealer) {
        outcome_ = isNaturalBlackjack(playerCards_, playerCount_) ? Outcome::Blackjack : Outcome::Win;
        const Money win = outcome_ == Outcome::Blackjack ? (bet() * 3) / 2 : bet();
        addMoney(win);
        lastWin_ = win;
      } else if (player == dealer) {
        outcome_ = Outcome::Push;
        lastWin_ = 0;
      } else {
        outcome_ = Outcome::Lose;
        loseBet();
      }
      clampBet();
      screen_ = Screen::BlackjackResult;
      dirty_ = true;
    }

    bool isRed(uint8_t n) const {
      static const uint8_t reds[] = {1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36};
      for (uint8_t i = 0; i < sizeof(reds); i++) {
        if (reds[i] == n) return true;
      }
      return false;
    }

    void markSlotCell(uint8_t col, uint8_t row) {
      if (col < 5 && row < 3) {
        slotWinMask_ |= static_cast<uint16_t>(1U << (row * 5 + col));
      }
    }

    void startRoulette() {
      activeGame_ = GameId::Roulette;
      memset(rouletteBets_, 0, sizeof(rouletteBets_));
      rouletteBet_ = RouletteBet::Red;
      rouletteWon_ = false;
      outcome_ = Outcome::None;
      lastWin_ = 0;
      screen_ = Screen::RouletteBet;
      dirty_ = true;
    }

    bool rouletteBetWins(RouletteBet bet, uint8_t number) const {
      if (number == 0) return bet == RouletteBet::Zero;
      switch (bet) {
        case RouletteBet::Red: return isRed(number);
        case RouletteBet::Black: return !isRed(number);
        case RouletteBet::Odd: return (number % 2) == 1;
        case RouletteBet::Even: return (number % 2) == 0;
        case RouletteBet::Low: return number <= 18;
        case RouletteBet::High: return number >= 19;
        case RouletteBet::Zero: return false;
      }
      return false;
    }

    void startRouletteSpin(uint16_t playerInitials) {
      const Money totalWager = rouletteTotalWager();
      if (totalWager == 0 || !takeWager(totalWager)) return;
      currentInitials_ = playerInitials;
      rouletteNumber_ = static_cast<uint8_t>(rand32() % 37);
      rouletteSpinMs_ = 0;
      rouletteWon_ = false;
      screen_ = Screen::RouletteSpin;
      dirty_ = true;
    }

    void updateRouletteSpin(uint32_t deltaMs) {
      rouletteSpinMs_ += deltaMs;
      if (rouletteSpinMs_ < 1700) {
        dirty_ = true;
        return;
      }
      const Money totalWager = rouletteTotalWager();
      Money payout = 0;
      for (uint8_t i = 0; i < 7; ++i) {
        const Money wager = rouletteBets_[i];
        if (wager == 0) continue;
        const RouletteBet placed = static_cast<RouletteBet>(i);
        if (rouletteBetWins(placed, rouletteNumber_)) {
          payout += wager * (placed == RouletteBet::Zero ? 36UL : 2UL);
        }
      }
      rouletteWon_ = payout > totalWager;
      outcome_ = payout > totalWager
                     ? Outcome::Win
                     : (payout == totalWager ? Outcome::Push : Outcome::Lose);
      pay(payout, currentInitials_);
      screen_ = Screen::RouletteResult;
      dirty_ = true;
    }

    uint8_t rollSlotSymbol(bool modern) {
      if (!modern) {
        return static_cast<uint8_t>(rand32() % 5);
      }
      const uint8_t r = static_cast<uint8_t>(rand32() % 100);
      if (r < 18) return 0;
      if (r < 36) return 1;
      if (r < 52) return 2;
      if (r < 66) return 3;
      if (r < 78) return 4;
      if (r < 88) return 5;
      if (r < 95) return 6;
      return 7;
    }

    void startSlotsSpin(bool modern) {
      activeGame_ = GameId::Slots;
      slotCols_ = modern ? 5 : 3;
      slotWinMask_ = 0;
      if (freeSpins_ > 0) {
        freeSpins_--;
      } else if (!takeWager(slotBet())) {
        return;
      }
      slotScatters_ = 0;
      for (uint8_t x = 0; x < 5; x++) {
        for (uint8_t y = 0; y < 3; y++) {
          slots_[x][y] = rollSlotSymbol(modern);
          if (modern && slots_[x][y] == 7) {
            slotScatters_++;
          }
        }
      }
      slotModern_ = modern;
      slotSpinMs_ = 0;
      screen_ = Screen::SlotsSpinning;
      dirty_ = true;
    }

    uint8_t slotLineRun(const uint8_t* rows, uint8_t& target) const {
      target = 255;
      uint8_t run = 0;
      for (uint8_t col = 0; col < 5; col++) {
        const uint8_t symbol = slots_[col][rows[col]];
        if (symbol == 7) {
          break;
        }
        if (target == 255 && symbol != 6) {
          target = symbol;
        }
        if (target == 255 || symbol == target || symbol == 6) {
          run++;
        } else {
          break;
        }
      }
      if (target == 255) {
        target = 6;
      }
      return run;
    }

    Money slotLinePay(uint8_t run, uint8_t symbol) const {
      if (run < 3) return 0;
      Money base = 1;
      if (symbol >= 4) base = 2;
      if (symbol == 5) base = 3;
      if (symbol == 6) base = 4;
      if (run == 4) return base * 2;
      if (run >= 5) return base * 5;
      return base;
    }

    void updateSlotsSpin(uint32_t deltaMs) {
      slotSpinMs_ += deltaMs;
      if (slotSpinMs_ < 1400) {
        dirty_ = true;
        return;
      }
      settleSlots();
    }

    void settleSlots() {
      Money multiplier = 0;
      slotWinMask_ = 0;
      if (slotModern_) {
        const uint8_t lines[5][5] = {
            {1,1,1,1,1},
            {0,0,0,0,0},
            {2,2,2,2,2},
            {0,1,2,1,0},
            {2,1,0,1,2},
        };
        for (uint8_t line = 0; line < 5; line++) {
          uint8_t target = 0;
          const uint8_t run = slotLineRun(lines[line], target);
          const Money linePay = slotLinePay(run, target);
          if (linePay > 0) {
            multiplier += linePay;
            for (uint8_t col = 0; col < run && col < 5; col++) {
              markSlotCell(col, lines[line][col]);
            }
          }
        }
        if (slotScatters_ >= 3) {
          freeSpins_ = static_cast<uint8_t>(freeSpins_ + 3);
          multiplier += static_cast<Money>(slotScatters_ - 1);
          for (uint8_t x = 0; x < 5; x++) {
            for (uint8_t y = 0; y < 3; y++) {
              if (slots_[x][y] == 7) {
                markSlotCell(x, y);
              }
            }
          }
        }
      } else {
        const uint8_t a = slots_[0][1];
        const uint8_t b = slots_[1][1];
        const uint8_t c = slots_[2][1];
        if (a == b && b == c) {
          multiplier = static_cast<uint16_t>((a + 2) * 4);
          markSlotCell(0, 1);
          markSlotCell(1, 1);
          markSlotCell(2, 1);
        } else if (a == b || b == c || a == c) {
          multiplier = 2;
          if (a == b) {
            markSlotCell(0, 1);
            markSlotCell(1, 1);
          }
          if (b == c) {
            markSlotCell(1, 1);
            markSlotCell(2, 1);
          }
          if (a == c) {
            markSlotCell(0, 1);
            markSlotCell(2, 1);
          }
        }
      }
      outcome_ = multiplier > 0 ? Outcome::Win : Outcome::Lose;
      pay(slotBet() * multiplier, currentInitials_);
      screen_ = Screen::SlotsResult;
      dirty_ = true;
    }

    void startHoldem() {
      activeGame_ = GameId::Holdem;
      screen_ = Screen::HoldemBet;
      outcome_ = Outcome::None;
      holdemTableConfigured_ = false;
      clampBet();
      dirty_ = true;
    }

    void dealHoldemHand(uint16_t playerInitials) {
      if (playerInitials != 0) currentInitials_ = playerInitials;
      holdemBigBlind_ = bet();
      holdemMaxBet_ = holdemBigBlind_ * 10;
      holdemRaiseAmount_ = holdemBigBlind_;
      holdemTableConfigured_ = true;
      beginHoldemHand(playerInitials);
    }

    void beginHoldemHand(uint16_t playerInitials) {
      activeGame_ = GameId::Holdem;
      if (playerInitials != 0) currentInitials_ = playerInitials;
      if (!holdemTableConfigured_) {
        startHoldem();
        return;
      }
      if (bankroll_ < holdemBigBlind_) {
        holdemTableConfigured_ = false;
        screen_ = Screen::HoldemBet;
        clampBet();
        holdemMessage_ = "Choose a lower blind";
        dirty_ = true;
        return;
      }

      buildDeck();
      memset(holdemFolded_, 0, sizeof(holdemFolded_));
      memset(holdemAllIn_, 0, sizeof(holdemAllIn_));
      memset(holdemActed_, 0, sizeof(holdemActed_));
      memset(holdemStreetBet_, 0, sizeof(holdemStreetBet_));
      memset(holdemTotalBet_, 0, sizeof(holdemTotalBet_));
      memset(holdemSeatScores_, 0, sizeof(holdemSeatScores_));
      for (uint8_t card = 0; card < 2; ++card) {
        for (uint8_t seat = 0; seat < HOLDEM_SEATS; ++seat) {
          holdemCards_[seat][card] = drawCard();
        }
      }
      for (uint8_t i = 0; i < 5; ++i) holdemCommunity_[i] = drawCard();

      holdemDealerSeat_ = static_cast<uint8_t>((holdemDealerSeat_ + 1) % HOLDEM_SEATS);
      const uint8_t smallBlind = static_cast<uint8_t>((holdemDealerSeat_ + 1) % HOLDEM_SEATS);
      const uint8_t bigBlind = static_cast<uint8_t>((holdemDealerSeat_ + 2) % HOLDEM_SEATS);
      holdemPot_ = 0;
      holdemCommunityShown_ = 0;
      holdemCurrentBet_ = holdemBigBlind_;
      holdemStake_ = 0;
      holdemWinnerSeat_ = 255;
      holdemWinnerCount_ = 0;
      holdemWinningScore_ = 0;
      playerPokerScore_ = 0;
      dealerPokerScore_ = 0;
      outcome_ = Outcome::None;
      lastWin_ = 0;
      postHoldemChips(smallBlind, holdemBigBlind_ / 2);
      postHoldemChips(bigBlind, holdemBigBlind_);
      holdemActor_ = nextHoldemSeat(bigBlind, true);
      holdemAction_ = HoldemAction::Check;
      holdemRaiseAmount_ = holdemBigBlind_;
      holdemActionTimerMs_ = 0;
      holdemMessage_ = "Blinds posted";
      screen_ = Screen::HoldemAction;
      dirty_ = true;
    }

    void playHoldemAction(uint16_t playerInitials) {
      if (screen_ != Screen::HoldemAction || holdemActor_ != 0 || holdemFolded_[0]) return;
      if (playerInitials != 0) currentInitials_ = playerInitials;
      if (holdemAction_ == HoldemAction::Fold) {
        holdemFolded_[0] = true;
        holdemActed_[0] = true;
        holdemMessage_ = "You fold - table plays";
      } else if (holdemAction_ == HoldemAction::Raise) {
        const Money due = holdemCallAmount();
        Money raise = holdemRaiseAmount_ < holdemBigBlind_ ? holdemBigBlind_ : holdemRaiseAmount_;
        if (holdemCurrentBet_ + raise > holdemMaxBet_) raise = holdemMaxBet_ - holdemCurrentBet_;
        const Money availableRaise = bankroll_ > due ? bankroll_ - due : 0;
        if (raise > availableRaise) raise = availableRaise;
        if (raise == 0) {
          postHoldemChips(0, due);
          holdemMessage_ = due == 0 ? "You check" : "You call";
        } else {
          for (uint8_t seat = 0; seat < HOLDEM_SEATS; ++seat) {
            if (!holdemFolded_[seat] && !holdemAllIn_[seat]) holdemActed_[seat] = false;
          }
          holdemCurrentBet_ += raise;
          postHoldemChips(0, due + raise);
          holdemMessage_ = "You raise";
        }
        holdemActed_[0] = true;
      } else {
        const Money due = holdemCallAmount();
        postHoldemChips(0, due);
        holdemActed_[0] = true;
        holdemMessage_ = due == 0 ? "You check" : "You call";
      }
      advanceHoldemTurn(currentInitials_);
    }

    void updateHoldem(uint32_t deltaMs, uint16_t playerInitials) {
      if (holdemActor_ == 0 || screen_ != Screen::HoldemAction) return;
      holdemActionTimerMs_ = static_cast<uint16_t>(holdemActionTimerMs_ + deltaMs);
      if (holdemActionTimerMs_ < 520) return;
      holdemActionTimerMs_ = 0;
      playNpcHoldemTurn(holdemActor_);
      advanceHoldemTurn(playerInitials);
      dirty_ = true;
    }

    void postHoldemChips(uint8_t seat, Money amount) {
      seat %= HOLDEM_SEATS;
      if (amount == 0) return;
      if (seat == 0 && amount > bankroll_) {
        amount = bankroll_;
        holdemAllIn_[0] = true;
      }
      if (seat == 0) bankroll_ -= amount;
      holdemStreetBet_[seat] += amount;
      holdemTotalBet_[seat] += amount;
      holdemPot_ += amount;
      if (seat == 0) holdemStake_ += amount;
    }

    uint8_t nextHoldemSeat(uint8_t after, bool needsAction) const {
      for (uint8_t offset = 1; offset <= HOLDEM_SEATS; ++offset) {
        const uint8_t seat = static_cast<uint8_t>((after + offset) % HOLDEM_SEATS);
        if (holdemFolded_[seat] || holdemAllIn_[seat]) continue;
        if (!needsAction || !holdemActed_[seat] || holdemStreetBet_[seat] < holdemCurrentBet_) return seat;
      }
      return 255;
    }

    uint8_t activeHoldemSeats() const {
      uint8_t count = 0;
      for (uint8_t seat = 0; seat < HOLDEM_SEATS; ++seat) if (!holdemFolded_[seat]) count++;
      return count;
    }

    bool holdemBettingComplete() const {
      for (uint8_t seat = 0; seat < HOLDEM_SEATS; ++seat) {
        if (holdemFolded_[seat] || holdemAllIn_[seat]) continue;
        if (!holdemActed_[seat] || holdemStreetBet_[seat] < holdemCurrentBet_) return false;
      }
      return true;
    }

    void advanceHoldemTurn(uint16_t playerInitials) {
      if (activeHoldemSeats() == 1) {
        settleHoldemByFold(playerInitials);
        return;
      }
      if (holdemBettingComplete()) {
        advanceHoldemStreet(playerInitials);
        return;
      }
      const uint8_t next = nextHoldemSeat(holdemActor_, true);
      if (next == 255) {
        advanceHoldemStreet(playerInitials);
        return;
      }
      holdemActor_ = next;
      holdemActionTimerMs_ = 0;
      if (holdemActor_ == 0) holdemAction_ = HoldemAction::Check;
    }

    void advanceHoldemStreet(uint16_t playerInitials) {
      if (holdemCommunityShown_ == 0) holdemCommunityShown_ = 3;
      else if (holdemCommunityShown_ < 5) holdemCommunityShown_++;
      else {
        settleHoldem(playerInitials);
        return;
      }
      memset(holdemStreetBet_, 0, sizeof(holdemStreetBet_));
      memset(holdemActed_, 0, sizeof(holdemActed_));
      holdemCurrentBet_ = 0;
      holdemActor_ = nextHoldemSeat(holdemDealerSeat_, true);
      holdemActionTimerMs_ = 0;
      holdemMessage_ = holdemCommunityShown_ == 3 ? "The flop" : (holdemCommunityShown_ == 4 ? "The turn" : "The river");
      if (holdemActor_ == 255) settleHoldem(playerInitials);
    }

    uint8_t holdemStrength(uint8_t seat) const {
      const uint8_t a = rankValue(holdemCards_[seat][0]);
      const uint8_t b = rankValue(holdemCards_[seat][1]);
      if (holdemCommunityShown_ == 0) {
        uint8_t strength = static_cast<uint8_t>((a + b) * 2);
        if (a == b) strength = static_cast<uint8_t>(strength + 30);
        if (cardSuit(holdemCards_[seat][0]) == cardSuit(holdemCards_[seat][1])) strength += 8;
        const uint8_t gap = a > b ? a - b : b - a;
        if (gap <= 2) strength += 6;
        return strength > 100 ? 100 : strength;
      }
      uint8_t cards[7] = {holdemCards_[seat][0], holdemCards_[seat][1], 0, 0, 0, 0, 0};
      for (uint8_t i = 0; i < holdemCommunityShown_; ++i) cards[2 + i] = holdemCommunity_[i];
      const uint32_t score = evaluateBest(cards, static_cast<uint8_t>(2 + holdemCommunityShown_));
      const uint8_t category = scoreCategory(score);
      const uint8_t high = static_cast<uint8_t>((score >> 20) & 0x0F);
      return static_cast<uint8_t>(category * 10 + high * 2 > 100 ? 100 : category * 10 + high * 2);
    }

    void playNpcHoldemTurn(uint8_t seat) {
      const Money due = holdemCurrentBet_ > holdemStreetBet_[seat]
                            ? holdemCurrentBet_ - holdemStreetBet_[seat]
                            : 0;
      const uint8_t strength = holdemStrength(seat);
      const uint8_t jitter = static_cast<uint8_t>(rand32() % 21);
      const uint8_t foldLimit = seat == 1 ? 20 : (seat == 2 ? 46 : 32);
      const uint8_t raiseLimit = seat == 1 ? 50 : (seat == 2 ? 82 : 68);
      const uint8_t pressure = holdemBigBlind_ == 0 ? 0 : static_cast<uint8_t>((due / holdemBigBlind_) * 8);

      if (due > 0 && strength + jitter < foldLimit + pressure) {
        holdemFolded_[seat] = true;
        holdemActed_[seat] = true;
        holdemMessage_ = seat == 1 ? "Blaze folds" : (seat == 2 ? "Sage folds" : "Ace folds");
        return;
      }

      const bool canRaise = holdemCurrentBet_ + holdemBigBlind_ <= holdemMaxBet_;
      if (canRaise && strength + jitter >= raiseLimit) {
        const Money raise = seat == 1 && (rand32() & 1) ? holdemBigBlind_ * 2 : holdemBigBlind_;
        for (uint8_t other = 0; other < HOLDEM_SEATS; ++other) {
          if (!holdemFolded_[other] && !holdemAllIn_[other]) holdemActed_[other] = false;
        }
        const Money cappedRaise = holdemCurrentBet_ + raise > holdemMaxBet_
                                      ? holdemMaxBet_ - holdemCurrentBet_
                                      : raise;
        holdemCurrentBet_ += cappedRaise;
        postHoldemChips(seat, due + cappedRaise);
        holdemActed_[seat] = true;
        holdemMessage_ = seat == 1 ? "Blaze raises" : (seat == 2 ? "Sage raises" : "Ace raises");
      } else {
        postHoldemChips(seat, due);
        holdemActed_[seat] = true;
        holdemMessage_ = due == 0 ? "NPC checks" : "NPC calls";
      }
    }

    void settleHoldemByFold(uint16_t playerInitials) {
      uint8_t winner = 0;
      while (winner < HOLDEM_SEATS && holdemFolded_[winner]) winner++;
      holdemWinnerSeat_ = winner;
      holdemWinnerCount_ = 1;
      holdemWinningScore_ = 0;
      if (winner == 0) {
        outcome_ = Outcome::Win;
        pay(holdemPot_, playerInitials);
        holdemMessage_ = "Everyone folded";
      } else {
        outcome_ = Outcome::Lose;
        pay(0, playerInitials);
        holdemMessage_ = "You lose";
      }
      screen_ = Screen::HoldemResult;
      dirty_ = true;
    }

    void settleHoldem(uint16_t playerInitials) {
      holdemWinnerSeat_ = 255;
      holdemWinnerCount_ = 0;
      holdemWinningScore_ = 0;
      for (uint8_t seat = 0; seat < HOLDEM_SEATS; ++seat) {
        if (holdemFolded_[seat]) continue;
        uint8_t seven[7] = {holdemCards_[seat][0], holdemCards_[seat][1],
                            holdemCommunity_[0], holdemCommunity_[1], holdemCommunity_[2],
                            holdemCommunity_[3], holdemCommunity_[4]};
        holdemSeatScores_[seat] = evaluateBest(seven, 7);
        if (holdemSeatScores_[seat] > holdemWinningScore_) {
          holdemWinningScore_ = holdemSeatScores_[seat];
          holdemWinnerSeat_ = seat;
          holdemWinnerCount_ = 1;
        } else if (holdemSeatScores_[seat] == holdemWinningScore_) {
          holdemWinnerCount_++;
        }
      }
      playerPokerScore_ = holdemSeatScores_[0];
      dealerPokerScore_ = holdemWinnerSeat_ < HOLDEM_SEATS ? holdemSeatScores_[holdemWinnerSeat_] : 0;
      if (!holdemFolded_[0] && playerPokerScore_ == holdemWinningScore_) {
        outcome_ = holdemWinnerCount_ > 1 ? Outcome::Push : Outcome::Win;
        pay(holdemWinnerCount_ == 0 ? 0 : holdemPot_ / holdemWinnerCount_, playerInitials);
        holdemMessage_ = outcome_ == Outcome::Win ? "You win" : "Split pot";
      } else {
        outcome_ = Outcome::Lose;
        pay(0, playerInitials);
        holdemMessage_ = "You lose";
      }
      screen_ = Screen::HoldemResult;
      dirty_ = true;
    }

    void startVideoPoker() {
      activeGame_ = GameId::VideoPoker;
      screen_ = Screen::VideoBet;
      videoCursor_ = 0;
      memset(videoHeld_, 0, sizeof(videoHeld_));
      videoMultiplier_ = 0;
      videoScore_ = 0;
      videoAnimating_ = false;
      videoDrawPhase_ = false;
      videoAnimationMs_ = 0;
    }

    void dealVideoPoker(uint16_t playerInitials) {
      (void)playerInitials;
      if (!takeBet()) return;
      buildDeck();
      for (uint8_t i = 0; i < 5; i++) {
        videoCards_[i] = drawCard();
        videoHeld_[i] = false;
      }
      videoCursor_ = 0;
      videoMultiplier_ = 0;
      videoScore_ = 0;
      screen_ = Screen::VideoHold;
      videoAnimating_ = true;
      videoDrawPhase_ = false;
      videoAnimationMs_ = 0;
      dirty_ = true;
    }

    void drawVideoPoker(uint16_t playerInitials) {
      if (videoAnimating_) return;
      for (uint8_t i = 0; i < 5; i++) {
        if (!videoHeld_[i]) {
          videoCards_[i] = drawCard();
        }
      }
      videoAnimating_ = true;
      videoDrawPhase_ = true;
      videoAnimationMs_ = 0;
      dirty_ = true;
    }

    void updateVideoAnimation(uint32_t deltaMs, uint16_t playerInitials) {
      videoAnimationMs_ = static_cast<uint16_t>(videoAnimationMs_ + deltaMs);
      if (videoAnimationMs_ < 760) {
        dirty_ = true;
        return;
      }

      videoAnimating_ = false;
      if (!videoDrawPhase_) {
        dirty_ = true;
        return;
      }

      videoScore_ = evaluateFive(videoCards_);
      videoMultiplier_ = videoPokerMultiplier(videoScore_);
      outcome_ = videoMultiplier_ > 0 ? Outcome::Win : Outcome::Lose;
      pay(bet() * videoMultiplier_, playerInitials);
      screen_ = Screen::VideoResult;
      videoDrawPhase_ = false;
      dirty_ = true;
    }

    uint32_t rng_ = 0xA53C9E11UL;
    Money bankroll_ = START_BANKROLL;
    Money bestCashout_ = 0;
    uint16_t bestCashoutInitials_ = 0;
    Money lastCashout_ = 0;
    Money gameStartBankroll_ = START_BANKROLL;
    uint16_t currentInitials_ = 0;
    uint8_t betIndex_ = 0;
    Screen screen_ = Screen::Hub;
    uint8_t hubIndex_ = 0;
    GameId activeGame_ = GameId::Blackjack;
    Outcome outcome_ = Outcome::None;
    bool dirty_ = true;
    Money lastWin_ = 0;

    uint8_t deck_[52] = {};
    uint8_t deckRemaining_ = 0;
    uint8_t blackjackDeck_[104] = {};
    uint8_t blackjackDeckRemaining_ = 0;

    uint8_t playerCards_[7] = {};
    uint8_t dealerCards_[7] = {};
    uint8_t playerCount_ = 0;
    uint8_t dealerCount_ = 0;
    uint16_t actionTimerMs_ = 0;
    uint8_t dealStep_ = 0;

    RouletteBet rouletteBet_ = RouletteBet::Red;
    Money rouletteBets_[7] = {};
    uint8_t rouletteNumber_ = 0;
    uint16_t rouletteSpinMs_ = 0;
    bool rouletteWon_ = false;

    uint8_t slots_[5][3] = {};
    uint8_t slotCols_ = 3;
    uint8_t freeSpins_ = 0;
    uint8_t slotScatters_ = 0;
    uint16_t slotSpinMs_ = 0;
    uint16_t slotWinMask_ = 0;
    bool slotModern_ = false;

    uint8_t holdemCards_[HOLDEM_SEATS][2] = {};
    uint8_t holdemCommunity_[5] = {};
    uint8_t holdemCommunityShown_ = 0;
    HoldemAction holdemAction_ = HoldemAction::Check;
    Money holdemPot_ = 0;
    Money holdemStake_ = 0;
    uint32_t playerPokerScore_ = 0;
    uint32_t dealerPokerScore_ = 0;
    bool holdemTableConfigured_ = false;
    bool holdemFolded_[HOLDEM_SEATS] = {};
    bool holdemAllIn_[HOLDEM_SEATS] = {};
    bool holdemActed_[HOLDEM_SEATS] = {};
    Money holdemStreetBet_[HOLDEM_SEATS] = {};
    Money holdemTotalBet_[HOLDEM_SEATS] = {};
    uint32_t holdemSeatScores_[HOLDEM_SEATS] = {};
    Money holdemBigBlind_ = 5;
    Money holdemMaxBet_ = 50;
    Money holdemCurrentBet_ = 0;
    Money holdemRaiseAmount_ = 5;
    uint8_t holdemDealerSeat_ = 3;
    uint8_t holdemActor_ = 0;
    uint8_t holdemWinnerSeat_ = 255;
    uint8_t holdemWinnerCount_ = 0;
    uint16_t holdemActionTimerMs_ = 0;
    uint32_t holdemWinningScore_ = 0;
    const char* holdemMessage_ = "Choose a blind";

    uint8_t videoCards_[5] = {};
    bool videoHeld_[5] = {};
    uint8_t videoCursor_ = 0;
    uint16_t videoMultiplier_ = 0;
    uint32_t videoScore_ = 0;
    bool videoAnimating_ = false;
    bool videoDrawPhase_ = false;
    uint16_t videoAnimationMs_ = 0;
};

}
