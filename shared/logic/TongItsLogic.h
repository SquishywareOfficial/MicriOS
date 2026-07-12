#pragma once

#include <stdint.h>
#include <string.h>

namespace TongIts {

constexpr uint8_t PLAYER_COUNT = 3;
constexpr uint8_t HUMAN = 0;
constexpr uint8_t MAX_HAND = 16;
constexpr uint8_t MAX_MELDS = 6;
constexpr uint8_t MAX_MELD_CARDS = 13;
constexpr uint8_t MAX_SUGGESTIONS = 8;

enum class Phase : uint8_t {
  Draw,
  Action,
  GameOver
};

enum class View : uint8_t {
  Hand,
  Piles,
  Melds,
  Table
};

enum class DrawSource : uint8_t {
  Stock,
  Discard
};

enum class MeldType : uint8_t {
  Set,
  Run
};

enum class EndReason : uint8_t {
  None,
  TongIts,
  StockEmpty
};

struct Meld {
  uint8_t cards[MAX_MELD_CARDS] = {};
  uint8_t count = 0;
  MeldType type = MeldType::Set;
};

inline uint8_t cardRank(uint8_t card) {
  return static_cast<uint8_t>(card % 13);
}

inline uint8_t cardSuit(uint8_t card) {
  return static_cast<uint8_t>(card / 13);
}

inline char rankChar(uint8_t card) {
  static const char ranks[] = {'A', '2', '3', '4', '5', '6', '7', '8', '9', 'T', 'J', 'Q', 'K'};
  return ranks[cardRank(card)];
}

inline char suitChar(uint8_t card) {
  static const char suits[] = {'C', 'D', 'H', 'S'};
  return suits[cardSuit(card) % 4];
}

inline uint8_t cardPoints(uint8_t card) {
  const uint8_t rank = cardRank(card);
  if (rank == 0) return 1;
  if (rank >= 9) return 10;
  return static_cast<uint8_t>(rank + 1);
}

class Logic {
  public:
    void seed(uint32_t value) {
      rng_ = value == 0 ? 0x544F4E47UL : value;
    }

    void startRound() {
      buildDeck();
      memset(handCount_, 0, sizeof(handCount_));
      memset(exposedCount_, 0, sizeof(exposedCount_));
      discardCount_ = 0;
      selectedHand_ = 0;
      selectedMeld_ = 0;
      selectedSource_ = DrawSource::Stock;
      view_ = View::Hand;
      phase_ = Phase::Draw;
      endReason_ = EndReason::None;
      winner_ = HUMAN;
      lastStockTaker_ = HUMAN;
      setMessage("Draw a card");

      for (uint8_t round = 0; round < 12; round++) {
        for (uint8_t player = 0; player < PLAYER_COUNT; player++) {
          addCard(player, drawStock());
        }
      }
      discard_[discardCount_++] = drawStock();
      for (uint8_t player = 0; player < PLAYER_COUNT; player++) {
        sortHand(player);
      }
      refreshSuggestions();
    }

    Phase phase() const { return phase_; }
    View view() const { return view_; }
    DrawSource selectedSource() const { return selectedSource_; }
    const char* message() const { return message_; }
    EndReason endReason() const { return endReason_; }
    uint8_t winner() const { return winner_; }
    uint8_t stockCount() const { return stockRemaining_; }
    uint8_t discardCount() const { return discardCount_; }
    uint8_t discardTop() const { return discardCount_ == 0 ? 255 : discard_[discardCount_ - 1]; }
    uint8_t handCount(uint8_t player) const { return handCount_[player % PLAYER_COUNT]; }
    uint8_t handCard(uint8_t player, uint8_t index) const { return hands_[player % PLAYER_COUNT][index % MAX_HAND]; }
    uint8_t selectedHand() const { return selectedHand_; }
    uint8_t exposedMeldCount(uint8_t player) const { return exposedCount_[player % PLAYER_COUNT]; }
    uint8_t exposedCardCount(uint8_t player, uint8_t meld) const {
      return exposed_[player % PLAYER_COUNT][meld % MAX_MELDS].count;
    }
    uint8_t exposedCard(uint8_t player, uint8_t meld, uint8_t index) const {
      return exposed_[player % PLAYER_COUNT][meld % MAX_MELDS].cards[index % MAX_MELD_CARDS];
    }
    uint8_t suggestionCount() const { return suggestionCount_; }
    uint8_t selectedMeld() const { return selectedMeld_; }
    uint8_t suggestionCardCount(uint8_t index) const { return suggestions_[index % MAX_SUGGESTIONS].count; }
    uint8_t suggestionCard(uint8_t suggestion, uint8_t cardIndex) const {
      return suggestions_[suggestion % MAX_SUGGESTIONS].cards[cardIndex % MAX_MELD_CARDS];
    }
    MeldType suggestionType(uint8_t index) const { return suggestions_[index % MAX_SUGGESTIONS].type; }
    uint8_t tableOptionCount() const { return static_cast<uint8_t>(totalExposedMelds() + 1); }
    uint8_t selectedTableOption() const { return selectedTable_; }
    bool tableOptionIsExit(uint8_t option) const { return option >= totalExposedMelds(); }
    bool tableOptionMeld(uint8_t option, uint8_t& player, uint8_t& meld) const {
      return mapTableOption(option, player, meld);
    }
    bool selectedTableIsExit() const { return tableOptionIsExit(selectedTable_); }
    bool selectedTableMeld(uint8_t& player, uint8_t& meld) const {
      return mapTableOption(selectedTable_, player, meld);
    }
    bool selectedHandCanLayoff() const {
      if (phase_ != Phase::Action || handCount_[HUMAN] == 0 || selectedTableIsExit()) return false;
      uint8_t player = 0;
      uint8_t meld = 0;
      if (!selectedTableMeld(player, meld)) return false;
      return canLayoffCard(hands_[HUMAN][selectedHand_], exposed_[player][meld]);
    }

    uint8_t deadwood(uint8_t player) const {
      uint8_t total = 0;
      for (uint8_t i = 0; i < handCount_[player % PLAYER_COUNT]; i++) {
        total = static_cast<uint8_t>(total + cardPoints(hands_[player % PLAYER_COUNT][i]));
      }
      return total;
    }

    bool humanOpened() const {
      return exposedCount_[HUMAN] > 0;
    }

    void nextView() {
      view_ = static_cast<View>((static_cast<uint8_t>(view_) + 1) % 4);
      if (view_ == View::Melds) {
        refreshSuggestions();
      } else if (view_ == View::Table) {
        clampTableSelection();
      }
    }

    void next() {
      if (phase_ == Phase::GameOver) {
        startRound();
        return;
      }

      if (view_ == View::Piles) {
        selectedSource_ = selectedSource_ == DrawSource::Stock ? DrawSource::Discard : DrawSource::Stock;
        return;
      }

      if (view_ == View::Melds) {
        refreshSuggestions();
        if (suggestionCount_ > 0) {
          selectedMeld_ = static_cast<uint8_t>((selectedMeld_ + 1) % suggestionCount_);
        }
        return;
      }

      if (view_ == View::Table) {
        nextTable();
        return;
      }

      if (view_ == View::Hand && handCount_[HUMAN] > 0) {
        selectedHand_ = static_cast<uint8_t>((selectedHand_ + 1) % handCount_[HUMAN]);
      }
    }

    void previousTable() {
      clampTableSelection();
      const uint8_t count = tableOptionCount();
      if (count == 0) return;
      selectedTable_ = selectedTable_ == 0 ? static_cast<uint8_t>(count - 1) : static_cast<uint8_t>(selectedTable_ - 1);
    }

    void exitTable() {
      view_ = View::Hand;
      setMessage("Back to hand");
    }

    void select() {
      if (phase_ == Phase::GameOver) {
        startRound();
        return;
      }

      if (view_ == View::Table) {
        selectTable();
        return;
      }

      if (phase_ == Phase::Draw) {
        drawForHuman();
        return;
      }

      if (view_ == View::Melds) {
        exposeSelectedMeld();
        return;
      }

      discardSelectedHumanCard();
    }

  private:
    uint32_t rand32() {
      rng_ ^= rng_ << 13;
      rng_ ^= rng_ >> 17;
      rng_ ^= rng_ << 5;
      return rng_;
    }

    void setMessage(const char* text) {
      strncpy(message_, text, sizeof(message_) - 1);
      message_[sizeof(message_) - 1] = '\0';
    }

    void buildDeck() {
      for (uint8_t i = 0; i < 52; i++) {
        deck_[i] = i;
      }
      stockRemaining_ = 52;
      for (uint8_t i = 0; i < 52; i++) {
        const uint8_t j = static_cast<uint8_t>(rand32() % 52);
        const uint8_t tmp = deck_[i];
        deck_[i] = deck_[j];
        deck_[j] = tmp;
      }
    }

    uint8_t drawStock() {
      if (stockRemaining_ == 0) {
        return 255;
      }
      return deck_[--stockRemaining_];
    }

    void addCard(uint8_t player, uint8_t card) {
      if (card == 255 || handCount_[player] >= MAX_HAND) {
        return;
      }
      hands_[player][handCount_[player]++] = card;
    }

    void sortHand(uint8_t player) {
      for (uint8_t i = 0; i + 1 < handCount_[player]; i++) {
        for (uint8_t j = i + 1; j < handCount_[player]; j++) {
          const uint8_t a = hands_[player][i];
          const uint8_t b = hands_[player][j];
          const uint8_t keyA = static_cast<uint8_t>(cardSuit(a) * 13 + cardRank(a));
          const uint8_t keyB = static_cast<uint8_t>(cardSuit(b) * 13 + cardRank(b));
          if (keyB < keyA) {
            hands_[player][i] = b;
            hands_[player][j] = a;
          }
        }
      }
    }

    int8_t findCardBySuitRank(uint8_t player, uint8_t suit, uint8_t rank) const {
      for (uint8_t i = 0; i < handCount_[player]; i++) {
        if (cardSuit(hands_[player][i]) == suit && cardRank(hands_[player][i]) == rank) {
          return static_cast<int8_t>(i);
        }
      }
      return -1;
    }

    void removeHandIndex(uint8_t player, uint8_t index) {
      if (index >= handCount_[player]) return;
      for (uint8_t i = index; i + 1 < handCount_[player]; i++) {
        hands_[player][i] = hands_[player][i + 1];
      }
      handCount_[player]--;
    }

    bool removeCardValue(uint8_t player, uint8_t card) {
      for (uint8_t i = 0; i < handCount_[player]; i++) {
        if (hands_[player][i] == card) {
          removeHandIndex(player, i);
          return true;
        }
      }
      return false;
    }

    void addSuggestion(MeldType type, const uint8_t* cards, uint8_t count, Meld* out, uint8_t& outCount) const {
      if (outCount >= MAX_SUGGESTIONS || count < 3) return;
      out[outCount].type = type;
      out[outCount].count = count > MAX_MELD_CARDS ? MAX_MELD_CARDS : count;
      for (uint8_t i = 0; i < out[outCount].count; i++) {
        out[outCount].cards[i] = cards[i];
      }
      outCount++;
    }

    uint8_t findSuggestionsFor(uint8_t player, Meld* out) const {
      uint8_t outCount = 0;

      for (uint8_t rank = 0; rank < 13; rank++) {
        uint8_t cards[4] = {};
        uint8_t count = 0;
        for (uint8_t i = 0; i < handCount_[player]; i++) {
          if (cardRank(hands_[player][i]) == rank) {
            cards[count++] = hands_[player][i];
          }
        }
        if (count >= 3) {
          addSuggestion(MeldType::Set, cards, count, out, outCount);
        }
      }

      for (uint8_t suit = 0; suit < 4; suit++) {
        uint8_t rank = 0;
        while (rank < 13) {
          uint8_t cards[MAX_MELD_CARDS] = {};
          uint8_t count = 0;
          uint8_t scan = rank;
          while (scan < 13 && findCardBySuitRank(player, suit, scan) >= 0) {
            const int8_t index = findCardBySuitRank(player, suit, scan);
            if (count < MAX_MELD_CARDS) {
              cards[count] = hands_[player][index];
            }
            count++;
            scan++;
          }
          if (count >= 3) {
            addSuggestion(MeldType::Run, cards, count, out, outCount);
            rank = scan;
          } else {
            rank++;
          }
        }
      }

      return outCount;
    }

    bool meldContainsCard(const Meld& meld, uint8_t card) const {
      for (uint8_t i = 0; i < meld.count; i++) {
        if (meld.cards[i] == card) return true;
      }
      return false;
    }

    bool findMeldContainingCard(uint8_t player, uint8_t card, Meld& out) const {
      Meld temp[MAX_SUGGESTIONS] = {};
      const uint8_t count = findSuggestionsFor(player, temp);
      for (uint8_t i = 0; i < count; i++) {
        if (meldContainsCard(temp[i], card)) {
          out = temp[i];
          return true;
        }
      }
      return false;
    }

    uint8_t totalExposedMelds() const {
      uint8_t total = 0;
      for (uint8_t player = 0; player < PLAYER_COUNT; player++) {
        total = static_cast<uint8_t>(total + exposedCount_[player]);
      }
      return total;
    }

    bool mapTableOption(uint8_t option, uint8_t& player, uint8_t& meld) const {
      uint8_t cursor = option;
      for (uint8_t p = 0; p < PLAYER_COUNT; p++) {
        if (cursor < exposedCount_[p]) {
          player = p;
          meld = cursor;
          return true;
        }
        cursor = static_cast<uint8_t>(cursor - exposedCount_[p]);
      }
      return false;
    }

    void clampTableSelection() {
      const uint8_t count = tableOptionCount();
      if (count == 0 || selectedTable_ >= count) {
        selectedTable_ = 0;
      }
    }

    void nextTable() {
      clampTableSelection();
      const uint8_t count = tableOptionCount();
      if (count == 0) return;
      selectedTable_ = static_cast<uint8_t>((selectedTable_ + 1) % count);
    }

    bool canLayoffCard(uint8_t card, const Meld& meld) const {
      if (card == 255 || meld.count == 0 || meld.count >= MAX_MELD_CARDS || meldContainsCard(meld, card)) {
        return false;
      }

      if (meld.type == MeldType::Set) {
        return meld.count < 4 && cardRank(card) == cardRank(meld.cards[0]);
      }

      if (cardSuit(card) != cardSuit(meld.cards[0])) {
        return false;
      }

      uint8_t low = cardRank(meld.cards[0]);
      uint8_t high = low;
      for (uint8_t i = 1; i < meld.count; i++) {
        const uint8_t rank = cardRank(meld.cards[i]);
        if (rank < low) low = rank;
        if (rank > high) high = rank;
      }
      const uint8_t rank = cardRank(card);
      return (low > 0 && rank == low - 1) || (high < 12 && rank == high + 1);
    }

    void addCardToMeld(Meld& meld, uint8_t card) {
      if (meld.count >= MAX_MELD_CARDS) return;
      if (meld.type == MeldType::Set) {
        meld.cards[meld.count++] = card;
        return;
      }

      uint8_t insertAt = meld.count;
      const uint8_t rank = cardRank(card);
      for (uint8_t i = 0; i < meld.count; i++) {
        if (rank < cardRank(meld.cards[i])) {
          insertAt = i;
          break;
        }
      }
      for (uint8_t i = meld.count; i > insertAt; i--) {
        meld.cards[i] = meld.cards[i - 1];
      }
      meld.cards[insertAt] = card;
      meld.count++;
    }

    void refreshSuggestions() {
      suggestionCount_ = findSuggestionsFor(HUMAN, suggestions_);
      if (selectedMeld_ >= suggestionCount_) {
        selectedMeld_ = 0;
      }
    }

    void exposeMeld(uint8_t player, const Meld& meld) {
      if (exposedCount_[player] >= MAX_MELDS || meld.count < 3) return;
      const uint8_t slot = exposedCount_[player]++;
      exposed_[player][slot] = meld;
      for (uint8_t i = 0; i < meld.count; i++) {
        removeCardValue(player, meld.cards[i]);
      }
      sortHand(player);
    }

    bool discardCardCanMeld(uint8_t player) {
      if (discardCount_ == 0 || handCount_[player] >= MAX_HAND) return false;
      const uint8_t top = discardTop();
      addCard(player, top);
      Meld meld;
      const bool canMeld = findMeldContainingCard(player, top, meld);
      removeCardValue(player, top);
      sortHand(player);
      return canMeld;
    }

    void drawForHuman() {
      uint8_t card = 255;
      if (selectedSource_ == DrawSource::Discard && discardCount_ > 0) {
        if (!discardCardCanMeld(HUMAN)) {
          setMessage("Discard must meld");
          view_ = View::Piles;
          return;
        }
        card = discard_[--discardCount_];
        addCard(HUMAN, card);
        sortHand(HUMAN);
        Meld meld;
        if (findMeldContainingCard(HUMAN, card, meld)) {
          exposeMeld(HUMAN, meld);
          setMessage("Discard melded");
        } else {
          setMessage("Discard failed");
        }
      } else {
        card = drawStock();
        lastStockTaker_ = HUMAN;
        addCard(HUMAN, card);
        sortHand(HUMAN);
        setMessage("Drew stock");
      }
      selectedHand_ = handCount_[HUMAN] == 0 ? 0 : static_cast<uint8_t>(handCount_[HUMAN] - 1);
      phase_ = Phase::Action;
      view_ = View::Hand;
      refreshSuggestions();
      clampTableSelection();
      if (handCount_[HUMAN] == 0) {
        endWithTongIts(HUMAN);
      }
    }

    void exposeSelectedMeld() {
      refreshSuggestions();
      if (suggestionCount_ == 0) {
        setMessage("No meld ready");
        return;
      }
      exposeMeld(HUMAN, suggestions_[selectedMeld_]);
      selectedHand_ = 0;
      refreshSuggestions();
      setMessage("Meld exposed");
      if (handCount_[HUMAN] == 0) {
        endWithTongIts(HUMAN);
      }
    }

    void discardSelectedHumanCard() {
      if (phase_ != Phase::Action || handCount_[HUMAN] == 0) return;
      if (selectedHand_ >= handCount_[HUMAN]) selectedHand_ = 0;
      discard_[discardCount_++] = hands_[HUMAN][selectedHand_];
      removeHandIndex(HUMAN, selectedHand_);
      if (selectedHand_ >= handCount_[HUMAN] && selectedHand_ > 0) selectedHand_--;
      refreshSuggestions();
      if (handCount_[HUMAN] == 0) {
        endWithTongIts(HUMAN);
        return;
      }
      runBots();
      if (phase_ != Phase::GameOver) {
        phase_ = Phase::Draw;
        view_ = View::Piles;
        setMessage("Your draw");
      }
    }

    void selectTable() {
      clampTableSelection();
      if (selectedTableIsExit()) {
        exitTable();
        return;
      }
      if (phase_ != Phase::Action) {
        setMessage("Draw first");
        return;
      }
      if (handCount_[HUMAN] == 0) return;

      uint8_t player = 0;
      uint8_t meld = 0;
      if (!selectedTableMeld(player, meld)) {
        setMessage("No meld");
        return;
      }

      const uint8_t card = hands_[HUMAN][selectedHand_];
      if (!canLayoffCard(card, exposed_[player][meld])) {
        setMessage("Card won't fit");
        return;
      }

      addCardToMeld(exposed_[player][meld], card);
      removeHandIndex(HUMAN, selectedHand_);
      if (selectedHand_ >= handCount_[HUMAN] && selectedHand_ > 0) selectedHand_--;
      refreshSuggestions();
      setMessage(player == HUMAN ? "Added to meld" : "Sapaw placed");
      if (handCount_[HUMAN] == 0) {
        endWithTongIts(HUMAN);
      }
    }

    uint8_t chooseBotDiscard(uint8_t player) const {
      uint8_t best = 0;
      uint8_t bestPoints = 0;
      for (uint8_t i = 0; i < handCount_[player]; i++) {
        const uint8_t points = cardPoints(hands_[player][i]);
        if (points >= bestPoints) {
          bestPoints = points;
          best = i;
        }
      }
      return best;
    }

    void runBots() {
      for (uint8_t player = 1; player < PLAYER_COUNT && phase_ != Phase::GameOver; player++) {
        if (stockRemaining_ == 0) {
          endByStock();
          return;
        }

        const bool canTakeDiscard = discardCardCanMeld(player);
        if (canTakeDiscard && (rand32() % 4) != 0) {
          const uint8_t card = discard_[--discardCount_];
          addCard(player, card);
          sortHand(player);
          Meld meld;
          if (findMeldContainingCard(player, card, meld)) {
            exposeMeld(player, meld);
          }
        } else {
          addCard(player, drawStock());
          lastStockTaker_ = player;
          sortHand(player);
        }

        Meld temp[MAX_SUGGESTIONS] = {};
        const uint8_t count = findSuggestionsFor(player, temp);
        if (count > 0 && (exposedCount_[player] == 0 || deadwood(player) > 25 || (rand32() % 3) == 0)) {
          exposeMeld(player, temp[0]);
        }

        if (handCount_[player] == 0) {
          endWithTongIts(player);
          return;
        }

        const uint8_t discardIndex = chooseBotDiscard(player);
        discard_[discardCount_++] = hands_[player][discardIndex];
        removeHandIndex(player, discardIndex);
        if (handCount_[player] == 0) {
          endWithTongIts(player);
          return;
        }
      }
      if (stockRemaining_ == 0) {
        endByStock();
      }
    }

    void endWithTongIts(uint8_t player) {
      phase_ = Phase::GameOver;
      endReason_ = EndReason::TongIts;
      winner_ = player;
      setMessage(player == HUMAN ? "Tong-its!" : "Bot tong-its");
    }

    void endByStock() {
      phase_ = Phase::GameOver;
      endReason_ = EndReason::StockEmpty;
      winner_ = lastStockTaker_;
      uint8_t best = 255;
      for (uint8_t player = 0; player < PLAYER_COUNT; player++) {
        const uint8_t score = deadwood(player);
        if (score < best || (score == best && player == lastStockTaker_)) {
          best = score;
          winner_ = player;
        }
      }
      setMessage("Stock empty");
    }

    uint32_t rng_ = 0x544F4E47UL;
    uint8_t deck_[52] = {};
    uint8_t stockRemaining_ = 0;
    uint8_t discard_[52] = {};
    uint8_t discardCount_ = 0;
    uint8_t hands_[PLAYER_COUNT][MAX_HAND] = {};
    uint8_t handCount_[PLAYER_COUNT] = {};
    Meld exposed_[PLAYER_COUNT][MAX_MELDS] = {};
    uint8_t exposedCount_[PLAYER_COUNT] = {};
    Meld suggestions_[MAX_SUGGESTIONS] = {};
    uint8_t suggestionCount_ = 0;
    uint8_t selectedMeld_ = 0;
    uint8_t selectedHand_ = 0;
    uint8_t selectedTable_ = 0;
    uint8_t lastStockTaker_ = HUMAN;
    uint8_t winner_ = HUMAN;
    Phase phase_ = Phase::Draw;
    View view_ = View::Hand;
    DrawSource selectedSource_ = DrawSource::Stock;
    EndReason endReason_ = EndReason::None;
    char message_[28] = {};
};

}
