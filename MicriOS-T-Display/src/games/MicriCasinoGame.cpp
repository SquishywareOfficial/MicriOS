#include "MicriCasinoGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <math.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
Preferences casinoPrefs;
constexpr const char* CASINO_NS = "casino";

String money(Casino::Money value) {
  if (value >= 1000000UL) {
    return "$" + String(static_cast<unsigned long>(value / 1000000UL)) + "M";
  }
  if (value >= 10000UL) {
    return "$" + String(static_cast<unsigned long>(value / 1000UL)) + "K";
  }
  return "$" + String(static_cast<unsigned long>(value));
}

uint16_t slotColor(uint8_t symbol) {
  switch (symbol % 8) {
    case 0: return TFT_YELLOW;
    case 1: return TFT_MAGENTA;
    case 2: return TFT_RED;
    case 3: return TFT_CYAN;
    case 4: return TFT_GREEN;
    case 5: return TFT_ORANGE;
    case 6: return TFT_WHITE;
    default: return TFT_SKYBLUE;
  }
}

char slotChar(uint8_t symbol) {
  static const char symbols[] = {'7', 'B', 'C', 'D', 'L', 'W', '*', '$'};
  return symbols[symbol % sizeof(symbols)];
}

const char* holdemStreetLabel(uint8_t shown) {
  if (shown == 0) return "Pre-flop";
  if (shown <= 3) return "The flop";
  if (shown == 4) return "The turn";
  return "The river";
}
}

MicriCasinoGame::MicriCasinoGame(uint32_t width, uint32_t height)
    : App("Micri Casino", width, height) {}

bool MicriCasinoGame::hasCustomOverlay() const {
  return true;
}

uint16_t MicriCasinoGame::runningRenderIntervalMs() const {
  return 50;
}

uint16_t MicriCasinoGame::staticRenderIntervalMs() const {
  return 1000;
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

void MicriCasinoGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  ensureLoaded();
  logic_.tick(deltaMs, playerInitials());
  if (b1.click) {
    logic_.next(true);
    saveBankroll();
  }
  if (b2.click) {
    logic_.previous();
    saveBankroll();
  }
  if (b1.longPress) {
    logic_.select(true, playerInitials());
    saveBankroll();
  }
}

template <typename Canvas>
void MicriCasinoGame::drawShell(Canvas& canvas, const char* title) {
  canvas.fillScreen(TFT_BLACK);
  TDisplayUi::header(canvas, title, TFT_GOLD, money(logic_.bankroll()).c_str());
  TDisplayUi::footer(canvas, "B1 next  Hold ok  B2 back");
}

template <typename Canvas>
void MicriCasinoGame::drawCard(Canvas& canvas, int x, int y, uint8_t card, bool hidden) {
  canvas.fillRoundRect(x, y, 25, 31, 3, hidden ? TFT_DARKGREY : TFT_WHITE);
  canvas.drawRoundRect(x, y, 25, 31, 3, TFT_LIGHTGREY);
  canvas.setTextDatum(TL_DATUM);
  if (hidden) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_BLACK, TFT_DARKGREY);
    canvas.drawString("?", x + 10, y + 12);
    return;
  }
  char label[2] = {Casino::rankChar(card), '\0'};
  const uint8_t suit = Casino::cardSuit(card);
  const uint16_t color = suit == 1 || suit == 2 ? TFT_RED : TFT_BLACK;
  canvas.setTextSize(2);
  canvas.setTextColor(color, TFT_WHITE);
  canvas.drawString(label, x + (label[0] == 'T' ? 7 : 6), y + 4);
  drawSuit(canvas, x + 13, y + 21, suit);
}

template <typename Canvas>
void MicriCasinoGame::drawSuit(Canvas& canvas, int x, int y, uint8_t suit) {
  const uint16_t color = suit == 1 || suit == 2 ? TFT_RED : TFT_BLACK;
  if (suit == 1) {
    canvas.fillCircle(x - 3, y - 2, 3, color);
    canvas.fillCircle(x + 3, y - 2, 3, color);
    canvas.fillTriangle(x - 7, y, x + 7, y, x, y + 8, color);
  } else if (suit == 2) {
    canvas.fillTriangle(x, y - 8, x - 6, y, x + 6, y, color);
    canvas.fillTriangle(x, y + 8, x - 6, y, x + 6, y, color);
  } else if (suit == 3) {
    canvas.fillCircle(x - 4, y, 3, color);
    canvas.fillCircle(x + 4, y, 3, color);
    canvas.fillCircle(x, y - 5, 3, color);
    canvas.fillTriangle(x - 3, y + 3, x + 3, y + 3, x, y + 8, color);
  } else {
    canvas.fillTriangle(x, y - 8, x - 7, y + 2, x + 7, y + 2, color);
    canvas.fillCircle(x - 4, y + 2, 3, color);
    canvas.fillCircle(x + 4, y + 2, 3, color);
    canvas.fillTriangle(x - 3, y + 4, x + 3, y + 4, x, y + 8, color);
  }
}

void MicriCasinoGame::drawStart(TFT_eSPI& tft) {
  ensureLoaded();
  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    drawShell(canvas, "Micri Casino");
    if (showStartScorePage()) {
      char dotted[4];
      PlayerProfile::unpackDottedInitials(logic_.bestInitials(), dotted);
      TDisplayUi::centered(canvas, "Top Bank", 45, 2, TFT_LIGHTGREY);
      TDisplayUi::largeValue(canvas, money(logic_.bestBankroll()), 65, TFT_GREEN);
      TDisplayUi::centered(canvas, dotted, 108, 1, TFT_WHITE);
    } else if (showStartPromptPage()) {
      TDisplayUi::largeValue(canvas, "PRESS", 45, TFT_GREEN);
      TDisplayUi::centered(canvas, "Start with $200 credits", 94, 1, TFT_LIGHTGREY);
    } else {
      drawCard(canvas, 28, 45, 0);
      drawCard(canvas, 58, 39, 12);
      canvas.drawRoundRect(104, 43, 46, 53, 4, TFT_GOLD);
      canvas.fillRect(112, 51, 30, 8, TFT_RED);
      canvas.fillRoundRect(111, 66, 8, 8, 2, TFT_YELLOW);
      canvas.fillRoundRect(124, 66, 8, 8, 2, TFT_GREEN);
      canvas.fillRoundRect(137, 66, 8, 8, 2, TFT_CYAN);
      canvas.drawCircle(181, 62, 13, TFT_RED);
      canvas.drawCircle(197, 74, 13, TFT_BLUE);
      canvas.drawCircle(169, 82, 13, TFT_GREEN);
      TDisplayUi::centered(canvas, "fictional credits only", 106, 1, TFT_LIGHTGREY);
    }
  });
}

void MicriCasinoGame::drawRunning(TFT_eSPI& tft) {
  ensureLoaded();
  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    switch (logic_.screen()) {
      case Casino::Screen::Hub: drawHub(canvas); break;
      case Casino::Screen::BlackjackShuffling:
      case Casino::Screen::BlackjackBet:
      case Casino::Screen::BlackjackDealing:
      case Casino::Screen::BlackjackTurn:
      case Casino::Screen::BlackjackDealerReveal:
      case Casino::Screen::BlackjackDealerTurn:
      case Casino::Screen::BlackjackResult: drawBlackjack(canvas); break;
      case Casino::Screen::RouletteBet:
      case Casino::Screen::RouletteStake:
      case Casino::Screen::RouletteSpin:
      case Casino::Screen::RouletteResult: drawRoulette(canvas); break;
      case Casino::Screen::SlotsIdle:
      case Casino::Screen::SlotsSpinning:
      case Casino::Screen::SlotsResult: drawSlots(canvas); break;
      case Casino::Screen::HoldemBet:
      case Casino::Screen::HoldemAction:
      case Casino::Screen::HoldemResult: drawHoldem(canvas); break;
      case Casino::Screen::VideoBet:
      case Casino::Screen::VideoHold:
      case Casino::Screen::VideoResult: drawVideoPoker(canvas); break;
      case Casino::Screen::Broke: drawBroke(canvas); break;
      case Casino::Screen::CashOut: drawCashOut(canvas); break;
    }
  });
  logic_.clearDirty();
}

template <typename Canvas>
void MicriCasinoGame::drawHub(Canvas& canvas) {
  drawShell(canvas, "Micri Casino");
  const uint8_t selected = logic_.hubIndex();
  for (uint8_t row = 0; row < 5; row++) {
    const int y = 34 + row * 15;
    const bool active = selected == row;
    canvas.fillRect(12, y, 126, 13, active ? TFT_GOLD : TFT_DARKGREY);
    canvas.setTextSize(1);
    canvas.setTextColor(active ? TFT_BLACK : TFT_WHITE, active ? TFT_GOLD : TFT_DARKGREY);
    canvas.drawString(logic_.hubLabel(row), 18, y + 3);
  }
  canvas.fillRect(150, 50, 74, 22, selected == 5 ? TFT_GOLD : TFT_DARKGREY);
  canvas.setTextSize(1);
  canvas.setTextColor(selected == 5 ? TFT_BLACK : TFT_LIGHTGREY, selected == 5 ? TFT_GOLD : TFT_DARKGREY);
  canvas.drawString("Cash Out", 157, 57);
  canvas.fillRect(150, 80, 74, 22, selected == 6 ? TFT_RED : TFT_DARKGREY);
  canvas.setTextColor(selected == 6 ? TFT_WHITE : TFT_LIGHTGREY, selected == 6 ? TFT_RED : TFT_DARKGREY);
  canvas.drawString("Reset", 157, 87);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString("Top " + money(logic_.bestBankroll()), 12, 113);
}

template <typename Canvas>
void MicriCasinoGame::drawBlackjack(Canvas& canvas) {
  drawShell(canvas, "Blackjack");
  if (logic_.screen() == Casino::Screen::BlackjackShuffling) {
    canvas.fillRoundRect(79, 44, 34, 46, 4, TFT_NAVY);
    canvas.drawRoundRect(79, 44, 34, 46, 4, TFT_WHITE);
    canvas.fillRoundRect(127, 44, 34, 46, 4, TFT_DARKGREEN);
    canvas.drawRoundRect(127, 44, 34, 46, 4, TFT_WHITE);
    TDisplayUi::centered(canvas, "Shuffling", 96, 2, TFT_YELLOW);
    TDisplayUi::footer(canvas, "Two-deck shoe");
    return;
  }

  if (logic_.screen() == Casino::Screen::BlackjackBet) {
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("Bet", 35, 48);
    TDisplayUi::largeValue(canvas, money(logic_.bet()), 63, TFT_YELLOW);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString(String(logic_.blackjackDeckRemaining()) + " cards left", 86, 105);
    TDisplayUi::footer(canvas, "B1 tap deal  Hold bet");
    return;
  }

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas.drawString("Dealer", 10, 36);
  for (uint8_t i = 0; i < logic_.dealerCount() && i < 5; i++) {
    drawCard(canvas, 78 + i * 26, 35, logic_.dealerCards()[i], logic_.blackjackHideDealer() && i == 0);
  }
  if (!logic_.blackjackHideDealer() && logic_.dealerCount() > 0) {
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
    canvas.drawString(String(logic_.blackjackDealerValue()), 215, 42);
  }
  canvas.setTextColor(TFT_GREEN, TFT_BLACK);
  canvas.drawString("Player", 10, 75);
  for (uint8_t i = 0; i < logic_.playerCount() && i < 5; i++) {
    drawCard(canvas, 78 + i * 26, 74, logic_.playerCards()[i]);
  }
  if (logic_.playerCount() > 0) {
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
    canvas.drawString(String(logic_.blackjackPlayerValue()), 215, 81);
  }
  TDisplayUi::footer(canvas, logic_.blackjackFooterText());
}

template <typename Canvas>
void MicriCasinoGame::drawRoulette(Canvas& canvas) {
  drawShell(canvas, "Roulette");
  if (logic_.screen() == Casino::Screen::RouletteBet) {
    TDisplayUi::largeValue(canvas, Casino::rouletteBetName(logic_.rouletteBet()), 42, TFT_GOLD);
    TDisplayUi::centered(canvas, "Choose pocket", 91, 2, TFT_WHITE);
    TDisplayUi::footer(canvas, "B1 type  Hold stake  B2 hub");
    return;
  }
  if (logic_.screen() == Casino::Screen::RouletteStake) {
    TDisplayUi::largeValue(canvas, "Bet " + money(logic_.bet()), 42, TFT_GOLD);
    TDisplayUi::centered(canvas, Casino::rouletteBetName(logic_.rouletteBet()), 91, 2, TFT_WHITE);
    TDisplayUi::footer(canvas, "B1 stake  Hold spin  B2 hub");
    return;
  }
  const uint8_t displayNumber = logic_.screen() == Casino::Screen::RouletteSpin ? logic_.rouletteDisplayNumber() : logic_.rouletteNumber();
  const bool red = displayNumber != 0 &&
                   (displayNumber == 1 || displayNumber == 3 || displayNumber == 5 ||
                    displayNumber == 7 || displayNumber == 9 || displayNumber == 12 ||
                    displayNumber == 14 || displayNumber == 16 || displayNumber == 18 ||
                    displayNumber == 19 || displayNumber == 21 || displayNumber == 23 ||
                    displayNumber == 25 || displayNumber == 27 || displayNumber == 30 ||
                    displayNumber == 32 || displayNumber == 34 || displayNumber == 36);
  const uint16_t color = displayNumber == 0 ? TFT_GREEN : (red ? TFT_RED : TFT_DARKGREY);
  canvas.fillCircle(70, 70, 36, color);
  canvas.drawCircle(70, 70, 39, TFT_WHITE);
  for (uint8_t i = 0; i < 12; i++) {
    const float a = (i * 30.0f) * 0.0174533f;
    canvas.drawLine(70, 70, 70 + cos(a) * 35, 70 + sin(a) * 35, TFT_DARKGREY);
  }
  if (logic_.screen() == Casino::Screen::RouletteSpin) {
    const float angle = (logic_.rouletteSpinMs() * 0.024f);
    canvas.fillCircle(70 + cos(angle) * 43, 70 + sin(angle) * 43, 4, TFT_WHITE);
  }
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextSize(3);
  canvas.setTextColor(TFT_WHITE, color);
  canvas.drawString(String(displayNumber), 70, 70);
  canvas.setTextDatum(TL_DATUM);
  if (logic_.screen() == Casino::Screen::RouletteSpin) {
    TDisplayUi::pill(canvas, 130, 48, "SPIN", TFT_YELLOW);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("Ball is slowing...", 130, 78);
    return;
  }
  TDisplayUi::pill(canvas, 130, 48, logic_.rouletteWon() ? "WIN" : "LOSE", logic_.rouletteWon() ? TFT_GREEN : TFT_RED);
  canvas.setTextSize(2);
  canvas.setTextColor(TFT_GOLD, TFT_BLACK);
  canvas.drawString("Paid " + money(logic_.lastWin()), 130, 78);
}

template <typename Canvas>
void MicriCasinoGame::drawSlots(Canvas& canvas) {
  drawShell(canvas, "Slots");
  if (logic_.screen() == Casino::Screen::SlotsIdle) {
    TDisplayUi::largeValue(canvas, "Bet " + money(logic_.bet()), 42, TFT_GOLD);
    TDisplayUi::centered(canvas, "5x3 wilds scatters free spins", 94, 1, TFT_LIGHTGREY);
    TDisplayUi::footer(canvas, "B1 stake  Hold spin  B2 hub");
    return;
  }
  const int startX = 18;
  const int startY = 34;
  for (uint8_t x = 0; x < logic_.slotCols(); x++) {
    for (uint8_t y = 0; y < 3; y++) {
      const uint8_t symbol = logic_.slotDisplaySymbol(x, y);
      const int px = startX + x * 38;
      const int py = startY + y * 28;
      canvas.fillRoundRect(px, py, 30, 23, 3, slotColor(symbol));
      canvas.setTextSize(2);
      canvas.setTextColor(TFT_BLACK, slotColor(symbol));
      char label[2] = {slotChar(symbol), '\0'};
      canvas.drawString(label, px + 9, py + 4);
      if (!logic_.slotsSpinning() && logic_.lastWin() > 0 && logic_.slotWinCell(x, y)) {
        const uint16_t border = ((millis() / 180) % 2) == 0 ? TFT_WHITE : TFT_GOLD;
        canvas.drawRoundRect(px - 2, py - 2, 34, 27, 4, border);
        canvas.drawRoundRect(px - 3, py - 3, 36, 29, 4, border);
      }
    }
  }
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  if (logic_.slotsSpinning()) {
    TDisplayUi::pill(canvas, 164, 101, "SPIN", TFT_YELLOW);
  } else {
    canvas.drawString("Scatters " + String(logic_.scatters()) + "  Free " + String(logic_.freeSpins()), 15, 104);
    if (logic_.lastWin() > 0) {
      TDisplayUi::pill(canvas, 158, 101, ("Win " + money(logic_.lastWin())).c_str(), TFT_GREEN);
    }
  }
}

template <typename Canvas>
void MicriCasinoGame::drawHoldem(Canvas& canvas) {
  drawShell(canvas, "Holdem");
  if (logic_.screen() == Casino::Screen::HoldemBet) {
    TDisplayUi::largeValue(canvas, "Bet " + money(logic_.bet()), 42, TFT_GOLD);
    TDisplayUi::centered(canvas, "Texas Hold'em", 91, 2, TFT_WHITE);
    TDisplayUi::footer(canvas, "B1 stake  Hold deal  B2 hub");
    return;
  }
  canvas.fillRect(0, 29, width, height - TDisplayUi::FOOTER_H - 29, TFT_DARKGREEN);
  canvas.drawFastHLine(0, 29, width, TFT_GREEN);
  canvas.drawFastHLine(0, height - TDisplayUi::FOOTER_H - 1, width, TFT_GREEN);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  canvas.drawString("You", 12, 34);
  drawCard(canvas, 12, 47, logic_.holdemPlayer()[0]);
  drawCard(canvas, 42, 47, logic_.holdemPlayer()[1]);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  canvas.drawString(holdemStreetLabel(logic_.communityShown()), 84, 34);
  for (uint8_t i = 0; i < logic_.communityShown(); i++) {
    drawCard(canvas, 84 + i * 29, 47, logic_.holdemCommunity()[i]);
  }
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  canvas.drawString("Dealer", 12, 90);
  const bool hideDealer = logic_.screen() == Casino::Screen::HoldemAction;
  drawCard(canvas, 60, 83, logic_.holdemDealer()[0], hideDealer);
  drawCard(canvas, 90, 83, logic_.holdemDealer()[1], hideDealer);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_GOLD, TFT_DARKGREEN);
  canvas.drawString("Pot " + money(logic_.pot()), 150, 90);
  if (logic_.screen() == Casino::Screen::HoldemAction) {
    TDisplayUi::pill(canvas, 150, 99, Casino::holdemActionName(logic_.holdemAction()), TFT_GREEN);
  } else {
    TDisplayUi::pill(canvas, 150, 88, Casino::outcomeName(logic_.outcome()), logic_.lastWin() > 0 ? TFT_GREEN : TFT_RED);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    const char* winner = logic_.outcome() == Casino::Outcome::Lose ? "Dealer" : (logic_.outcome() == Casino::Outcome::Win ? "You" : "Push");
    canvas.drawString(String(winner) + ": " + Casino::pokerScoreName(logic_.holdemWinningScore()), 126, 109);
  }
}

template <typename Canvas>
void MicriCasinoGame::drawVideoPoker(Canvas& canvas) {
  drawShell(canvas, "Video Poker");
  if (logic_.screen() == Casino::Screen::VideoBet) {
    TDisplayUi::largeValue(canvas, "Bet " + money(logic_.bet()), 42, TFT_GOLD);
    TDisplayUi::centered(canvas, "Jacks or Better", 96, 2, TFT_LIGHTGREY);
    return;
  }
  for (uint8_t i = 0; i < 5; i++) {
    drawCard(canvas, 12 + i * 43, 43, logic_.videoCards()[i]);
    if (logic_.videoHeld(i)) {
      TDisplayUi::pill(canvas, 14 + i * 43, 78, "HOLD", TFT_GOLD);
    }
    if (logic_.screen() == Casino::Screen::VideoHold && logic_.videoCursor() == i) {
      canvas.drawRoundRect(9 + i * 43, 40, 31, 37, 4, TFT_GREEN);
    }
  }
  if (logic_.screen() == Casino::Screen::VideoHold) {
    if (logic_.videoCursor() == 5) {
      TDisplayUi::pill(canvas, 181, 86, "DRAW", TFT_GREEN);
    }
    TDisplayUi::centered(canvas, "B1 card/draw  hold mark", 112, 1, TFT_LIGHTGREY);
  } else {
    TDisplayUi::centered(canvas, Casino::pokerScoreName(logic_.videoScore()), 86, 2, logic_.lastWin() > 0 ? TFT_GREEN : TFT_RED);
    TDisplayUi::centered(canvas, "Paid " + money(logic_.lastWin()), 112, 1, TFT_GOLD);
  }
}

template <typename Canvas>
void MicriCasinoGame::drawBroke(Canvas& canvas) {
  drawShell(canvas, "Broke");
  TDisplayUi::largeValue(canvas, "$0", 42, TFT_RED);
  TDisplayUi::centered(canvas, "B1 resets to $200", 94, 2, TFT_WHITE);
}

template <typename Canvas>
void MicriCasinoGame::drawCashOut(Canvas& canvas) {
  drawShell(canvas, "Cash Out");
  TDisplayUi::centered(canvas, "Top Bank Saved", 42, 2, TFT_GOLD);
  TDisplayUi::largeValue(canvas, money(logic_.bankroll()), 65, TFT_GREEN);
  TDisplayUi::centered(canvas, "B1/B2 returns to casino", 108, 1, TFT_LIGHTGREY);
}

void MicriCasinoGame::drawEnd(TFT_eSPI& tft) {
  ensureLoaded();
  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    drawShell(canvas, "Micri Casino");
    TDisplayUi::centered(canvas, "Saved", 48, 3, TFT_GREEN);
    TDisplayUi::centered(canvas, "Bank " + money(logic_.bankroll()), 88, 2, TFT_GOLD);
  });
}
