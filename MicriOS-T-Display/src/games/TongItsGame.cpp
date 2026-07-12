#include "TongItsGame.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
String cardText(uint8_t card) {
  if (card == 255) return "--";
  char text[4] = {TongIts::rankChar(card), TongIts::suitChar(card), '\0'};
  return String(text);
}

uint16_t suitColor(uint8_t card) {
  const uint8_t suit = TongIts::cardSuit(card);
  return suit == 1 || suit == 2 ? TFT_RED : TFT_BLACK;
}

const char* meldTypeText(TongIts::MeldType type) {
  return type == TongIts::MeldType::Set ? "Set" : "Run";
}
}

TongItsGame::TongItsGame(uint32_t width, uint32_t height)
    : App("Tong-its", width, height) {}

bool TongItsGame::hasCustomOverlay() const {
  return true;
}

uint16_t TongItsGame::runningRenderIntervalMs() const {
  return 120;
}

uint16_t TongItsGame::staticRenderIntervalMs() const {
  return 1000;
}

bool TongItsGame::consumesButton2HoldInRunning() const {
  return true;
}

void TongItsGame::onAppReset() {
  logic_.seed(micros() ^ millis());
  logic_.startRound();
}

void TongItsGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)deltaMs;
  if (b2.longPress) {
    if (logic_.view() == TongIts::View::Table) {
      logic_.exitTable();
    } else {
      requestExitToMenu();
    }
    return;
  }

  if (b2.click) {
    if (logic_.view() == TongIts::View::Table) {
      logic_.previousTable();
    } else {
      logic_.nextView();
    }
  }
  if (b1.click) {
    logic_.next();
  }
  if (b1.longPress) {
    logic_.select();
  }
}

const char* TongItsGame::viewName() const {
  switch (logic_.view()) {
    case TongIts::View::Hand: return "Hand";
    case TongIts::View::Piles: return "Piles";
    case TongIts::View::Melds: return "Melds";
    case TongIts::View::Table: return "Table";
  }
  return "";
}

const char* TongItsGame::phaseName() const {
  switch (logic_.phase()) {
    case TongIts::Phase::Draw: return "Draw";
    case TongIts::Phase::Action: return "Act";
    case TongIts::Phase::GameOver: return "Done";
  }
  return "";
}

const char* TongItsGame::playerName(uint8_t player) const {
  switch (player) {
    case 0: return "You";
    case 1: return "Bot A";
    default: return "Bot B";
  }
}

template <typename Canvas>
void TongItsGame::drawShell(Canvas& canvas, const char* subtitle) {
  canvas.fillScreen(TFT_BLACK);
  TDisplayUi::header(canvas, "Tong-its", TFT_MAGENTA, subtitle);
  TDisplayUi::footer(canvas, "B1 next  Hold do  B2 view");
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(String(viewName()) + " / " + phaseName(), 12, 27);
  canvas.drawString(logic_.message(), 128, 27);
}

template <typename Canvas>
void TongItsGame::drawCard(Canvas& canvas, int x, int y, uint8_t card, bool selected, bool small) {
  const int w = small ? 20 : 24;
  const int h = small ? 25 : 31;
  canvas.fillRoundRect(x, y, w, h, 3, TFT_WHITE);
  canvas.drawRoundRect(x, y, w, h, 3, selected ? TFT_GOLD : TFT_LIGHTGREY);
  if (selected) {
    canvas.drawRoundRect(x - 2, y - 2, w + 4, h + 4, 4, TFT_GOLD);
  }
  canvas.setTextSize(small ? 1 : 2);
  canvas.setTextColor(suitColor(card), TFT_WHITE);
  canvas.drawString(String(TongIts::rankChar(card)), x + (small ? 6 : 7), y + (small ? 5 : 5));
  canvas.setTextSize(1);
  canvas.drawString(String(TongIts::suitChar(card)), x + (small ? 7 : 9), y + h - 9);
}

template <typename Canvas>
void TongItsGame::drawHand(Canvas& canvas) {
  drawShell(canvas, "your cards");
  const uint8_t count = logic_.handCount(TongIts::HUMAN);
  const uint8_t selected = logic_.selectedHand();
  uint8_t first = 0;
  if (count > 7 && selected > 3) {
    first = static_cast<uint8_t>(selected - 3);
    if (first + 7 > count) first = static_cast<uint8_t>(count - 7);
  }

  for (uint8_t i = 0; i < 7 && first + i < count; i++) {
    const uint8_t index = static_cast<uint8_t>(first + i);
    drawCard(canvas, 13 + i * 31, 51, logic_.handCard(TongIts::HUMAN, index), index == selected);
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  if (logic_.phase() == TongIts::Phase::Draw) {
    canvas.drawString("B2 to piles, hold B1 to draw stock", 15, 105);
  } else {
    canvas.drawString("B1 picks a card, hold discards it", 15, 105);
  }
  canvas.drawString("Deadwood " + String(logic_.deadwood(TongIts::HUMAN)), 164, 43);
}

template <typename Canvas>
void TongItsGame::drawPiles(Canvas& canvas) {
  drawShell(canvas, "draw piles");
  const bool stockSelected = logic_.selectedSource() == TongIts::DrawSource::Stock;
  canvas.fillRoundRect(32, 51, 62, 50, 5, stockSelected ? TFT_NAVY : TFT_DARKGREY);
  canvas.drawRoundRect(32, 51, 62, 50, 5, stockSelected ? TFT_GOLD : TFT_LIGHTGREY);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, stockSelected ? TFT_NAVY : TFT_DARKGREY);
  canvas.drawString("Stock", 45, 59);
  canvas.setTextSize(2);
  canvas.drawString(String(logic_.stockCount()), 54, 75);

  const bool discardSelected = logic_.selectedSource() == TongIts::DrawSource::Discard;
  canvas.fillRoundRect(142, 51, 62, 50, 5, discardSelected ? TFT_NAVY : TFT_DARKGREY);
  canvas.drawRoundRect(142, 51, 62, 50, 5, discardSelected ? TFT_GOLD : TFT_LIGHTGREY);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, discardSelected ? TFT_NAVY : TFT_DARKGREY);
  canvas.drawString("Discard", 153, 59);
  if (logic_.discardCount() > 0) {
    drawCard(canvas, 160, 72, logic_.discardTop(), false, true);
  } else {
    canvas.drawString("empty", 157, 78);
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(logic_.phase() == TongIts::Phase::Draw ? "B1 toggles, hold draws" : "Draw complete - discard from hand", 25, 109);
}

template <typename Canvas>
void TongItsGame::drawMelds(Canvas& canvas) {
  drawShell(canvas, "melds");
  const uint8_t count = logic_.suggestionCount();
  if (count == 0) {
    TDisplayUi::centered(canvas, "No meld ready", 61, 2, TFT_YELLOW);
    TDisplayUi::centered(canvas, "Try drawing or discard", 90, 1, TFT_LIGHTGREY);
    return;
  }

  const uint8_t selected = logic_.selectedMeld();
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_GOLD, TFT_BLACK);
  canvas.drawString(String(meldTypeText(logic_.suggestionType(selected))) + " " + String(selected + 1) + "/" + String(count), 18, 45);

  const uint8_t cardCount = logic_.suggestionCardCount(selected);
  const uint8_t visibleCards = cardCount > 5 ? 5 : cardCount;
  for (uint8_t i = 0; i < visibleCards; i++) {
    drawCard(canvas, 42 + i * 31, 65, logic_.suggestionCard(selected, i));
  }
  if (cardCount > visibleCards) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("+" + String(cardCount - visibleCards), 42 + visibleCards * 31, 78);
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString("B1 next meld, hold expose", 43, 109);
}

template <typename Canvas>
void TongItsGame::drawTable(Canvas& canvas) {
  drawShell(canvas, "table melds");
  TDisplayUi::footer(canvas, "B1/B2 move  Hold B1 sapaw  Hold B2 back");

  const uint8_t optionCount = logic_.tableOptionCount();
  const uint8_t selected = logic_.selectedTableOption();
  constexpr uint8_t VISIBLE_ROWS = 3;
  uint8_t first = 0;
  if (optionCount > VISIBLE_ROWS && selected > 1) {
    first = static_cast<uint8_t>(selected - 1);
    if (first + VISIBLE_ROWS > optionCount) {
      first = static_cast<uint8_t>(optionCount - VISIBLE_ROWS);
    }
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString("Card:", 12, 39);
  if (logic_.handCount(TongIts::HUMAN) > 0) {
    drawCard(canvas, 47, 34, logic_.handCard(TongIts::HUMAN, logic_.selectedHand()), false, true);
  } else {
    canvas.drawString("--", 50, 44);
  }

  for (uint8_t row = 0; row < VISIBLE_ROWS; row++) {
    const uint8_t option = static_cast<uint8_t>(first + row);
    if (option >= optionCount) break;

    const int y = 34 + row * 26;
    const bool isSelected = option == selected;
    const uint16_t bg = isSelected ? TFT_NAVY : TFT_BLACK;
    canvas.fillRoundRect(78, y, 151, 23, 4, bg);
    canvas.drawRoundRect(78, y, 151, 23, 4, isSelected ? TFT_GOLD : TFT_DARKGREY);

    if (logic_.tableOptionIsExit(option)) {
      canvas.setTextSize(1);
      canvas.setTextColor(isSelected ? TFT_GOLD : TFT_LIGHTGREY, bg);
      canvas.drawString("Exit meld view", 104, y + 8);
      continue;
    }

    uint8_t player = 0;
    uint8_t meld = 0;
    logic_.tableOptionMeld(option, player, meld);
    const uint16_t ownerColor = player == TongIts::HUMAN ? TFT_GREEN : TFT_CYAN;
    canvas.setTextSize(1);
    canvas.setTextColor(ownerColor, bg);
    canvas.drawString(playerName(player), 83, y + 8);

    const uint8_t cardCount = logic_.exposedCardCount(player, meld);
    const uint8_t visibleCards = cardCount > 5 ? 5 : cardCount;
    for (uint8_t i = 0; i < visibleCards; i++) {
      drawCard(canvas, 123 + i * 20, y + 1, logic_.exposedCard(player, meld, i), false, true);
    }
    if (cardCount > visibleCards) {
      canvas.setTextColor(TFT_LIGHTGREY, bg);
      canvas.drawString("+" + String(cardCount - visibleCards), 224, y + 8);
    }
  }

  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString("Page " + String(first / VISIBLE_ROWS + 1) + "  Deadwood " +
                    String(logic_.deadwood(TongIts::HUMAN)), 12, 107);
  if (logic_.selectedHandCanLayoff()) {
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.drawString("fits", 185, 107);
  } else if (!logic_.selectedTableIsExit()) {
    canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
    canvas.drawString("no fit", 178, 107);
  }
}

template <typename Canvas>
void TongItsGame::drawGameOver(Canvas& canvas) {
  drawShell(canvas, logic_.endReason() == TongIts::EndReason::TongIts ? "tong-its" : "stock empty");
  const bool humanWon = logic_.winner() == TongIts::HUMAN;
  TDisplayUi::centered(canvas, humanWon ? "YOU WIN" : "BOT WINS", 44, 3, humanWon ? TFT_GREEN : TFT_RED);
  TDisplayUi::centered(canvas, String(playerName(logic_.winner())) + " wins", 76, 2, TFT_GOLD);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString("Deadwood: You " + String(logic_.deadwood(0)) +
                    " A " + String(logic_.deadwood(1)) +
                    " B " + String(logic_.deadwood(2)), 20, 105);
}

void TongItsGame::drawRunning(TFT_eSPI& tft) {
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    if (logic_.phase() == TongIts::Phase::GameOver) {
      drawGameOver(canvas);
      return;
    }
    switch (logic_.view()) {
      case TongIts::View::Hand: drawHand(canvas); break;
      case TongIts::View::Piles: drawPiles(canvas); break;
      case TongIts::View::Melds: drawMelds(canvas); break;
      case TongIts::View::Table: drawTable(canvas); break;
    }
  });
}

void TongItsGame::drawStart(TFT_eSPI& tft) {
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    canvas.fillScreen(TFT_BLACK);
    TDisplayUi::header(canvas, "Tong-its", TFT_MAGENTA, "rummy table");
    if (showStartPromptPage()) {
      TDisplayUi::largeValue(canvas, "PRESS", 46, TFT_GREEN);
      TDisplayUi::centered(canvas, "B2 cycles views", 96, 1, TFT_LIGHTGREY);
    } else {
      drawCard(canvas, 55, 49, 0);
      drawCard(canvas, 86, 49, 1);
      drawCard(canvas, 117, 49, 2);
      drawCard(canvas, 151, 49, 26);
      TDisplayUi::centered(canvas, "Draw, meld, discard", 98, 1, TFT_LIGHTGREY);
    }
  });
}

void TongItsGame::drawEnd(TFT_eSPI& tft) {
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    drawGameOver(canvas);
  });
}
