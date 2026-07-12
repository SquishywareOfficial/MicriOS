#include "MicriCasinoGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
Preferences casinoPrefs;
constexpr const char* CASINO_NS = "casino";

void shortMoney(char* out, size_t outSize, Casino::Money value) {
  if (value >= 1000000UL) {
    snprintf(out, outSize, "$%luM", static_cast<unsigned long>(value / 1000000UL));
  } else if (value >= 10000UL) {
    snprintf(out, outSize, "$%luK", static_cast<unsigned long>(value / 1000UL));
  } else {
    snprintf(out, outSize, "$%lu", static_cast<unsigned long>(value));
  }
}
}

MicriCasinoGame::MicriCasinoGame(uint32_t width, uint32_t height, uint32_t left)
    : App("Micri Casino", width, height), left_(left) {}

bool MicriCasinoGame::hasCustomOverlay() const {
  return true;
}

void MicriCasinoGame::ensureLoaded() {
  if (loaded_) {
    return;
  }
  casinoPrefs.begin(CASINO_NS, true);
  const Casino::Money bank = casinoPrefs.getUInt("bank", Casino::START_BANKROLL);
  const Casino::Money best = casinoPrefs.getUInt("best", Casino::START_BANKROLL);
  const uint16_t initials = casinoPrefs.getUShort("init", PlayerProfile::defaultInitials());
  casinoPrefs.end();
  logic_.seed(micros() ^ millis());
  logic_.loadBankroll(bank, best, initials);
  savedBankroll_ = logic_.bankroll();
  savedBestBankroll_ = logic_.bestBankroll();
  savedBestInitials_ = logic_.bestInitials();
  loaded_ = true;
}

void MicriCasinoGame::saveBankroll() {
  if (!loaded_) {
    return;
  }
  if (savedBankroll_ == logic_.bankroll() &&
      savedBestBankroll_ == logic_.bestBankroll() &&
      savedBestInitials_ == logic_.bestInitials()) {
    return;
  }
  casinoPrefs.begin(CASINO_NS, false);
  casinoPrefs.putUInt("bank", logic_.bankroll());
  casinoPrefs.putUInt("best", logic_.bestBankroll());
  casinoPrefs.putUShort("init", logic_.bestInitials());
  casinoPrefs.end();
  savedBankroll_ = logic_.bankroll();
  savedBestBankroll_ = logic_.bestBankroll();
  savedBestInitials_ = logic_.bestInitials();
}

uint16_t MicriCasinoGame::playerInitials() const {
  return PlayerProfile::loadInitials();
}

void MicriCasinoGame::onAppReset() {
  ensureLoaded();
  logic_.resetToHub();
}

void MicriCasinoGame::onAppExit() {
  saveBankroll();
}

void MicriCasinoGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  ensureLoaded();
  logic_.tick(deltaMs, playerInitials());
  if (input.click) {
    logic_.next(false);
    saveBankroll();
  }
  if (input.longPress) {
    logic_.select(false, playerInitials());
    saveBankroll();
  }
}

void MicriCasinoGame::drawFrame(U8G2& u8g2, const char* title) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 8, title);
}

void MicriCasinoGame::drawMoney(U8G2& u8g2, int x, int y, const char* label, Casino::Money value) {
  char amount[10];
  shortMoney(amount, sizeof(amount), value);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(x, y, label);
  u8g2.drawStr(x + (label[0] == '\0' ? 0 : 18), y, amount);
}

void MicriCasinoGame::drawCardLine(U8G2& u8g2, int x, int y, const uint8_t* cards, uint8_t count, uint8_t visibleCount) {
  char label[4];
  u8g2.setFont(u8g2_font_4x6_tr);
  const uint8_t maxCards = count < visibleCount ? count : visibleCount;
  for (uint8_t i = 0; i < maxCards; i++) {
    Casino::cardLabel(cards[i], label, sizeof(label));
    u8g2.drawStr(x + i * 11, y, label);
  }
}

void MicriCasinoGame::drawBlackjackCards(U8G2& u8g2, int x, int y, const uint8_t* cards, uint8_t count, bool hideFirst) {
  const uint8_t visibleCards = min<uint8_t>(count, 6);
  char label[2] = {' ', '\0'};
  u8g2.setFont(u8g2_font_4x6_tr);
  for (uint8_t i = 0; i < visibleCards; i++) {
    u8g2.drawFrame(x + i * 8, y - 6, 7, 8);
    label[0] = hideFirst && i == 0 ? '?' : Casino::rankChar(cards[i]);
    u8g2.drawStr(x + i * 8 + 2, y, label);
  }
  if (count > visibleCards) {
    u8g2.drawStr(x + visibleCards * 8 + 1, y, "+");
  }
  if (!hideFirst && count > 0) {
    u8g2.setCursor(x + 48, y);
    u8g2.print(Casino::blackjackValue(cards, count));
  }
}

void MicriCasinoGame::drawStart(U8G2& u8g2) {
  ensureLoaded();
  drawFrame(u8g2, "Casino");
  if (showStartScorePage()) {
    char dotted[4];
    PlayerProfile::unpackDottedInitials(logic_.bestInitials(), dotted);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(4, 18, "Top Bank");
    drawMoney(u8g2, 4, 28, "", logic_.bestBankroll());
    u8g2.drawStr(44, 28, dotted);
  } else if (showStartPromptPage()) {
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(8, 20, "Press start");
    u8g2.drawStr(6, 32, "5 games $200");
  } else {
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(7, 19, "Cards Slots");
    u8g2.drawStr(13, 31, "Roulette");
  }
}

void MicriCasinoGame::drawRunning(U8G2& u8g2) {
  ensureLoaded();
  switch (logic_.screen()) {
    case Casino::Screen::Hub: drawHub(u8g2); break;
    case Casino::Screen::BlackjackShuffling:
    case Casino::Screen::BlackjackBet:
    case Casino::Screen::BlackjackDealing:
    case Casino::Screen::BlackjackTurn:
      case Casino::Screen::BlackjackDealerReveal:
      case Casino::Screen::BlackjackDealerTurn:
      case Casino::Screen::BlackjackResult: drawBlackjack(u8g2); break;
      case Casino::Screen::RouletteBet:
      case Casino::Screen::RouletteStake:
      case Casino::Screen::RouletteSpin:
      case Casino::Screen::RouletteResult: drawRoulette(u8g2); break;
      case Casino::Screen::SlotsIdle:
      case Casino::Screen::SlotsSpinning:
      case Casino::Screen::SlotsResult: drawSlots(u8g2); break;
      case Casino::Screen::HoldemBet:
      case Casino::Screen::HoldemAction:
      case Casino::Screen::HoldemResult: drawHoldem(u8g2); break;
    case Casino::Screen::VideoBet:
    case Casino::Screen::VideoHold:
    case Casino::Screen::VideoResult: drawVideoPoker(u8g2); break;
    case Casino::Screen::Broke: drawBroke(u8g2); break;
    case Casino::Screen::CashOut: drawCashOut(u8g2); break;
  }
  logic_.clearDirty();
}

void MicriCasinoGame::drawHub(U8G2& u8g2) {
  drawFrame(u8g2, "Casino");
  u8g2.setFont(u8g2_font_5x8_tr);
  const uint8_t index = logic_.hubIndex();
  if (index == 5) {
    u8g2.drawStr(13, 21, "Cash Out");
  } else if (index == 6) {
    u8g2.drawStr(13, 21, "Reset Bank");
  } else {
    u8g2.drawStr(6, 21, logic_.hubLabel(index));
  }
  drawMoney(u8g2, 3, 31, "Bank", logic_.bankroll());
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 38, "Tap next Hold ok");
}

void MicriCasinoGame::drawBlackjack(U8G2& u8g2) {
  drawFrame(u8g2, "Blackjack");
  drawMoney(u8g2, 3, 6, "", logic_.bankroll());
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(42, 6);
  u8g2.print("B");
  u8g2.print(logic_.bet());

  if (logic_.screen() == Casino::Screen::BlackjackShuffling) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(12, 20, "Shuffle");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(8, 32, "2 deck shoe");
    return;
  }

  if (logic_.screen() == Casino::Screen::BlackjackBet) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(17, 18, "Bet");
    u8g2.setCursor(38, 18);
    u8g2.print(logic_.bet());
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(2, 29);
    u8g2.print(logic_.blackjackDeckRemaining());
    u8g2.print(" cards");
    u8g2.drawStr(2, 38, "Tap deal Hold bet");
    return;
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(2, 15, "D");
  drawBlackjackCards(u8g2, 10, 15, logic_.dealerCards(), logic_.dealerCount(), logic_.blackjackHideDealer());
  u8g2.drawStr(2, 27, "P");
  drawBlackjackCards(u8g2, 10, 27, logic_.playerCards(), logic_.playerCount(), false);
  u8g2.drawStr(2, 38, logic_.blackjackFooterText());
}

void MicriCasinoGame::drawRoulette(U8G2& u8g2) {
  drawFrame(u8g2, "Roulette");
  u8g2.setFont(u8g2_font_5x8_tr);
  if (logic_.screen() == Casino::Screen::RouletteBet) {
    u8g2.drawStr(8, 22, Casino::rouletteBetName(logic_.rouletteBet()));
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(3, 34, "Tap type Hold bet");
  } else if (logic_.screen() == Casino::Screen::RouletteStake) {
    drawMoney(u8g2, 3, 22, "Bet", logic_.bet());
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(3, 34, "Tap stake Hold spin");
  } else if (logic_.screen() == Casino::Screen::RouletteSpin) {
    char number[8];
    snprintf(number, sizeof(number), "No.%u", logic_.rouletteDisplayNumber());
    u8g2.drawStr(4, 20, "Spin...");
    u8g2.drawStr(4, 31, number);
  } else {
    char number[8];
    snprintf(number, sizeof(number), "No.%u", logic_.rouletteNumber());
    u8g2.drawStr(4, 20, number);
    u8g2.drawStr(39, 20, logic_.rouletteWon() ? "WIN" : "LOSE");
    drawMoney(u8g2, 3, 34, "Paid", logic_.lastWin());
  }
  u8g2.setFont(u8g2_font_4x6_tr);
  if (logic_.screen() != Casino::Screen::RouletteBet && logic_.screen() != Casino::Screen::RouletteStake) {
    u8g2.drawStr(3, 38, "Tap next Hold ok");
  }
}

void MicriCasinoGame::drawSlots(U8G2& u8g2) {
  drawFrame(u8g2, "Slots");
  static const char symbols[] = {'7', 'B', 'C', 'D', 'L', 'W', '*', '$'};
  if (logic_.screen() == Casino::Screen::SlotsIdle) {
    drawMoney(u8g2, 4, 21, "Bet", logic_.bet());
    drawMoney(u8g2, 4, 31, "Bank", logic_.bankroll());
    u8g2.drawStr(3, 38, "Tap bet Hold spin");
    return;
  }
  u8g2.setFont(u8g2_font_logisoso16_tr);
  for (uint8_t i = 0; i < 3; i++) {
    const uint8_t symbol = logic_.slotDisplaySymbol(i, 1) % sizeof(symbols);
    char text[2] = {symbols[symbol], '\0'};
    u8g2.drawStr(12 + i * 18, 28, text);
    if (!logic_.slotsSpinning() && logic_.lastWin() > 0 && logic_.slotWinCell(i, 1)) {
      u8g2.drawFrame(10 + i * 18, 9, 15, 21);
    }
  }
  u8g2.setFont(u8g2_font_4x6_tr);
  if (logic_.slotsSpinning()) {
    u8g2.drawStr(3, 38, "Spinning...");
  } else if (logic_.lastWin() > 0) {
    drawMoney(u8g2, 3, 38, "Win", logic_.lastWin());
  } else {
    u8g2.drawStr(3, 38, "No win Tap again");
  }
}

void MicriCasinoGame::drawHoldem(U8G2& u8g2) {
  drawFrame(u8g2, "Holdem");
  if (logic_.screen() == Casino::Screen::HoldemBet) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(16, 17, "Bet");
    u8g2.setCursor(37, 17);
    u8g2.print(logic_.bet());
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(3, 31, "Tap stake");
    u8g2.drawStr(3, 38, "Hold deal");
    return;
  }
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 15, "You");
  drawCardLine(u8g2, 24, 15, logic_.holdemPlayer(), 2, 2);
  u8g2.drawStr(3, 23, "Flop");
  drawCardLine(u8g2, 24, 23, logic_.holdemCommunity(), logic_.communityShown(), 5);
  if (logic_.screen() == Casino::Screen::HoldemAction) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 35, Casino::holdemActionName(logic_.holdemAction()));
  } else {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 35, Casino::outcomeName(logic_.outcome()));
    drawMoney(u8g2, 41, 38, "", logic_.lastWin());
  }
}

void MicriCasinoGame::drawVideoPoker(U8G2& u8g2) {
  drawFrame(u8g2, "Video Poker");
  if (logic_.screen() == Casino::Screen::VideoBet) {
    drawMoney(u8g2, 4, 21, "Bet", logic_.bet());
    drawMoney(u8g2, 4, 31, "Bank", logic_.bankroll());
    u8g2.drawStr(3, 38, "Tap bet Hold deal");
    return;
  }
  drawCardLine(u8g2, 4, 17, logic_.videoCards(), 5, 5);
  u8g2.setFont(u8g2_font_4x6_tr);
  if (logic_.screen() == Casino::Screen::VideoHold) {
    for (uint8_t i = 0; i < 5; i++) {
      if (logic_.videoHeld(i)) {
        u8g2.drawStr(5 + i * 11, 26, "H");
      }
    }
    if (logic_.videoCursor() < 5) {
      u8g2.drawFrame(3 + logic_.videoCursor() * 11, 10, 11, 18);
    } else {
      u8g2.drawStr(46, 34, "DRAW");
    }
    u8g2.drawStr(3, 38, "Tap card Hold mark");
  } else {
    u8g2.drawStr(3, 29, Casino::pokerScoreName(logic_.videoScore()));
    drawMoney(u8g2, 3, 38, "Win", logic_.lastWin());
  }
}

void MicriCasinoGame::drawBroke(U8G2& u8g2) {
  drawFrame(u8g2, "Broke");
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(5, 19, "Bank is $0");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 31, "Tap reset to $200");
  u8g2.drawStr(3, 38, "Hold menu");
}

void MicriCasinoGame::drawCashOut(U8G2& u8g2) {
  drawFrame(u8g2, "Cash Out");
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(5, 17, "Top saved");
  drawMoney(u8g2, 4, 30, "Bank", logic_.bankroll());
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 38, "Tap back");
}

void MicriCasinoGame::drawEnd(U8G2& u8g2) {
  drawFrame(u8g2, "Casino");
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(9, 21, "Saved");
  drawMoney(u8g2, 4, 35, "Bank", logic_.bankroll());
}
