#include "TongItsGame.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "../../CydFramebuffer.h"
#include "../../CydUi.h"

namespace {
constexpr TouchUi::Rect TONG_START = {88, 157, 144, 39};
constexpr TouchUi::Rect TONG_ACTION = {88, 157, 144, 38};
constexpr TouchUi::Rect TONG_TABS[4] = {
    {8, 38, 72, 27}, {85, 38, 72, 27}, {162, 38, 72, 27}, {239, 38, 72, 27}};
constexpr TouchUi::Rect TONG_PILES[2] = {{28, 70, 120, 84}, {172, 70, 120, 84}};
constexpr TouchUi::Rect TONG_MELD_ROWS[3] = {
    {45, 72, 263, 34}, {45, 111, 263, 34}, {45, 150, 263, 34}};
constexpr TouchUi::Rect TONG_TABLE_ROWS[3] = {
    {66, 70, 242, 35}, {66, 109, 242, 35}, {66, 148, 242, 35}};
constexpr int16_t TONG_DISCARD_ACTION = 45;

TouchUi::Rect handCardRect(uint8_t index) {
  if (index < 7) {
    return {static_cast<int16_t>(8 + index * 44), 69, 42, 35};
  }
  return {static_cast<int16_t>(30 + (index - 7) * 44), 107, 42, 35};
}

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
  return phase() == AppPhase::Start ? 1000 : 0;
}

bool TongItsGame::consumesButton2HoldInRunning() const {
  return true;
}

bool TongItsGame::handleTouch(const TouchUi::TouchSample& sample,
                              const TouchUi::TouchEvent& event) {
  const TouchUi::Point point = TouchUi::currentPoint(sample, event);
  int16_t hit = TouchUi::NO_CONTROL;
  if (phase() == AppPhase::Start) {
    if (TONG_START.contains(point)) hit = 1;
  } else if (phase() == AppPhase::End || logic_.phase() == TongIts::Phase::GameOver) {
    if (TONG_ACTION.contains(point)) hit = 2;
  } else {
    hit = TouchUi::hitRectList(TONG_TABS, 4, point);
    if (hit >= 0) hit += 10;
    else if (logic_.view() == TongIts::View::Hand) {
      const uint8_t count = logic_.handCount(TongIts::HUMAN);
      for (uint8_t i = 0; i < count; ++i) {
        if (handCardRect(i).contains(point)) {
          hit = 20 + i;
          break;
        }
      }
      if (hit == TouchUi::NO_CONTROL && logic_.phase() == TongIts::Phase::Action &&
          TONG_ACTION.contains(point)) hit = TONG_DISCARD_ACTION;
    } else if (logic_.view() == TongIts::View::Piles && logic_.phase() == TongIts::Phase::Draw) {
      hit = TouchUi::hitRectList(TONG_PILES, 2, point);
      if (hit >= 0) hit += 40;
    } else if (logic_.view() == TongIts::View::Melds) {
      const uint8_t count = logic_.suggestionCount();
      const uint8_t selected = logic_.selectedMeld();
      uint8_t first = count > 3 && selected > 1 ? selected - 1 : 0;
      if (count > 3 && first + 3 > count) first = count - 3;
      const int16_t row = TouchUi::hitRectList(TONG_MELD_ROWS, 3, point);
      if (row >= 0 && first + row < count) hit = 50 + first + row;
    } else if (logic_.view() == TongIts::View::Table) {
      const uint8_t count = logic_.tableOptionCount();
      const uint8_t selected = logic_.selectedTableOption();
      uint8_t first = count > 3 && selected > 1 ? selected - 1 : 0;
      if (count > 3 && first + 3 > count) first = count - 3;
      const int16_t row = TouchUi::hitRectList(TONG_TABLE_ROWS, 3, point);
      if (row >= 0 && first + row < count) hit = 70 + first + row;
    }
  }

  const TouchUi::CaptureResult result = touchCapture_.update(sample, event, hit);
  if (!result.activated) return result.consumed;
  const int16_t id = result.id;
  if (id == 1) startRunning();
  else if (id == 2) logic_.startRound();
  else if (id >= 10 && id < 14) {
    logic_.setView(static_cast<TongIts::View>(id - 10));
  } else if (id >= 20 && id < 20 + TongIts::MAX_HAND) {
    logic_.setSelectedHand(static_cast<uint8_t>(id - 20));
  } else if (id == TONG_DISCARD_ACTION) {
    logic_.select();
  } else if (id == 40 || id == 41) {
    const TongIts::DrawSource source = id == 40 ? TongIts::DrawSource::Stock : TongIts::DrawSource::Discard;
    logic_.setSelectedSource(source);
    logic_.select();
  } else if (id >= 50 && id < 50 + TongIts::MAX_SUGGESTIONS) {
    logic_.setSelectedMeld(static_cast<uint8_t>(id - 50));
    logic_.select();
  } else if (id >= 70) {
    logic_.setSelectedTableOption(static_cast<uint8_t>(id - 70));
    logic_.select();
  }
  return true;
}

void TongItsGame::onAppReset() {
  touchCapture_.reset();
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
  CydUi::header(canvas, "Tong-its", TFT_MAGENTA, subtitle);
  const char* labels[] = {"Hand", "Piles", "Melds", "Table"};
  for (uint8_t i = 0; i < 4; ++i) {
    CydUi::touchTab(canvas, TONG_TABS[i], labels[i],
                    static_cast<uint8_t>(logic_.view()) == i, TFT_MAGENTA);
  }
}

template <typename Canvas>
void TongItsGame::drawCard(Canvas& canvas, int x, int y, uint8_t card, bool selected, bool small) {
  const int w = small ? 24 : 34;
  const int h = small ? 31 : 44;
  canvas.fillRoundRect(x, y, w, h, 3, TFT_WHITE);
  canvas.drawRoundRect(x, y, w, h, 3, selected ? TFT_GOLD : TFT_LIGHTGREY);
  if (selected) {
    canvas.drawRoundRect(x - 2, y - 2, w + 4, h + 4, 4, TFT_GOLD);
  }
  const bool ten = TongIts::rankChar(card) == 'T';
  canvas.setTextSize(small ? 1 : (ten ? 2 : 3));
  canvas.setTextColor(suitColor(card), TFT_WHITE);
  const String rank = ten ? String("10") : String(TongIts::rankChar(card));
  canvas.drawString(rank, x + (small ? (ten ? 6 : 8) : (ten ? 5 : 9)),
                    y + (small ? 7 : (ten ? 8 : 5)));
  canvas.setTextSize(1);
  canvas.drawString(String(TongIts::suitChar(card)), x + (small ? 9 : 14), y + h - 10);
}

template <typename Canvas>
void TongItsGame::drawHand(Canvas& canvas) {
  drawShell(canvas, "your cards");
  const uint8_t count = logic_.handCount(TongIts::HUMAN);
  const uint8_t selected = logic_.selectedHand();
  for (uint8_t i = 0; i < count; i++) {
    const TouchUi::Rect rect = handCardRect(i);
    drawCard(canvas, rect.x + 9, rect.y + 2,
             logic_.handCard(TongIts::HUMAN, i), i == selected, true);
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  if (logic_.phase() == TongIts::Phase::Draw)
    CydUi::centered(canvas, "Choose Stock or Discard", 145, 1, TFT_LIGHTGREY);
  else
    CydUi::touchAction(canvas, TONG_ACTION, "Discard Selected", TFT_ORANGE);
  canvas.drawString("Deadwood " + String(logic_.deadwood(TongIts::HUMAN)), 12, 145);
}

template <typename Canvas>
void TongItsGame::drawPiles(Canvas& canvas) {
  drawShell(canvas, "draw piles");
  const bool stockSelected = logic_.selectedSource() == TongIts::DrawSource::Stock;
  canvas.fillRoundRect(TONG_PILES[0].x, TONG_PILES[0].y, TONG_PILES[0].w, TONG_PILES[0].h, 7, stockSelected ? TFT_NAVY : TFT_DARKGREY);
  canvas.drawRoundRect(TONG_PILES[0].x, TONG_PILES[0].y, TONG_PILES[0].w, TONG_PILES[0].h, 7, stockSelected ? TFT_GOLD : TFT_LIGHTGREY);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, stockSelected ? TFT_NAVY : TFT_DARKGREY);
  canvas.drawString("Stock", 67, 80);
  canvas.setTextSize(4);
  canvas.drawString(String(logic_.stockCount()), 65, 105);

  const bool discardSelected = logic_.selectedSource() == TongIts::DrawSource::Discard;
  canvas.fillRoundRect(TONG_PILES[1].x, TONG_PILES[1].y, TONG_PILES[1].w, TONG_PILES[1].h, 7, discardSelected ? TFT_NAVY : TFT_DARKGREY);
  canvas.drawRoundRect(TONG_PILES[1].x, TONG_PILES[1].y, TONG_PILES[1].w, TONG_PILES[1].h, 7, discardSelected ? TFT_GOLD : TFT_LIGHTGREY);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, discardSelected ? TFT_NAVY : TFT_DARKGREY);
  canvas.drawString("Discard", 207, 80);
  if (logic_.discardCount() > 0) {
    drawCard(canvas, 218, 101, logic_.discardTop(), false, false);
  } else {
    canvas.drawString("empty", 213, 104);
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(logic_.phase() == TongIts::Phase::Draw ? "Tap a pile to draw" : "Draw complete - choose Hand", 75, 171);
}

template <typename Canvas>
void TongItsGame::drawMelds(Canvas& canvas) {
  drawShell(canvas, "melds");
  const uint8_t count = logic_.suggestionCount();
  if (count == 0) {
    CydUi::centered(canvas, "No meld ready", 72, 3, TFT_YELLOW);
    CydUi::centered(canvas, "Try drawing or discard", 121, 2, TFT_LIGHTGREY);
    return;
  }

  const uint8_t selected = logic_.selectedMeld();
  uint8_t first = count > 3 && selected > 1 ? selected - 1 : 0;
  if (count > 3 && first + 3 > count) first = count - 3;
  for (uint8_t row = 0; row < 3 && first + row < count; ++row) {
    const uint8_t index = first + row;
    const String label = String(meldTypeText(logic_.suggestionType(index))) +
                         " - " + String(logic_.suggestionCardCount(index)) + " cards";
    CydUi::touchAction(canvas, TONG_MELD_ROWS[row], label.c_str(), TFT_MAGENTA,
                       index == selected);
  }
}

template <typename Canvas>
void TongItsGame::drawTable(Canvas& canvas) {
  drawShell(canvas, "table melds");

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
  canvas.drawString("Card:", 12, 70);
  if (logic_.handCount(TongIts::HUMAN) > 0) {
    drawCard(canvas, 17, 84, logic_.handCard(TongIts::HUMAN, logic_.selectedHand()), false, false);
  } else {
    canvas.drawString("--", 22, 65);
  }

  for (uint8_t row = 0; row < VISIBLE_ROWS; row++) {
    const uint8_t option = static_cast<uint8_t>(first + row);
    if (option >= optionCount) break;

    const int y = 70 + row * 39;
    const bool isSelected = option == selected;
    const uint16_t bg = isSelected ? TFT_NAVY : TFT_BLACK;
    canvas.fillRoundRect(66, y, 242, 35, 5, bg);
    canvas.drawRoundRect(66, y, 242, 35, 5, isSelected ? TFT_GOLD : TFT_DARKGREY);

    if (logic_.tableOptionIsExit(option)) {
      canvas.setTextSize(1);
      canvas.setTextColor(isSelected ? TFT_GOLD : TFT_LIGHTGREY, bg);
      canvas.drawString("Exit meld view", 139, y + 13);
      continue;
    }

    uint8_t player = 0;
    uint8_t meld = 0;
    logic_.tableOptionMeld(option, player, meld);
    const uint16_t ownerColor = player == TongIts::HUMAN ? TFT_GREEN : TFT_CYAN;
    canvas.setTextSize(1);
    canvas.setTextColor(ownerColor, bg);
    canvas.drawString(playerName(player), 72, y + 13);

    const uint8_t cardCount = logic_.exposedCardCount(player, meld);
    const uint8_t visibleCards = cardCount > 7 ? 7 : cardCount;
    for (uint8_t i = 0; i < visibleCards; i++) {
      drawCard(canvas, 120 + i * 25, y + 2, logic_.exposedCard(player, meld, i), false, true);
    }
    if (cardCount > visibleCards) {
      canvas.setTextColor(TFT_LIGHTGREY, bg);
      canvas.drawString("+" + String(cardCount - visibleCards), 296, y + 13);
    }
  }

  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString("Deadwood " + String(logic_.deadwood(TongIts::HUMAN)), 12, 189);
  if (logic_.selectedHandCanLayoff()) {
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.drawString("fits", 263, 189);
  } else if (!logic_.selectedTableIsExit()) {
    canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
    canvas.drawString("no fit", 256, 189);
  }
}

template <typename Canvas>
void TongItsGame::drawGameOver(Canvas& canvas) {
  drawShell(canvas, logic_.endReason() == TongIts::EndReason::TongIts ? "tong-its" : "stock empty");
  const bool humanWon = logic_.winner() == TongIts::HUMAN;
  CydUi::centered(canvas, humanWon ? "YOU WIN" : "BOT WINS", 59, 4, humanWon ? TFT_GREEN : TFT_RED);
  CydUi::centered(canvas, String(playerName(logic_.winner())) + " wins", 112, 2, TFT_GOLD);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString("Deadwood: You " + String(logic_.deadwood(0)) +
                    " A " + String(logic_.deadwood(1)) +
                    " B " + String(logic_.deadwood(2)), 50, 139);
  CydUi::touchAction(canvas, TONG_ACTION, "Play Again", TFT_GREEN);
}

void TongItsGame::drawRunning(TFT_eSPI& tft) {
  CydFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
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
  CydFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    canvas.fillScreen(TFT_BLACK);
    CydUi::header(canvas, "Tong-its", TFT_MAGENTA, "rummy table");
    if (showStartPromptPage()) {
      CydUi::largeValue(canvas, "TONG-ITS", 61, TFT_GREEN);
      CydUi::centered(canvas, "Touch cards and piles", 135, 2, TFT_LIGHTGREY);
    } else {
      drawCard(canvas, 64, 65, 0);
      drawCard(canvas, 108, 65, 1);
      drawCard(canvas, 152, 65, 2);
      drawCard(canvas, 202, 65, 26);
      CydUi::centered(canvas, "Draw, meld, discard", 139, 2, TFT_LIGHTGREY);
    }
    CydUi::touchAction(canvas, TONG_START, "Deal Cards", TFT_GREEN);
  });
}

void TongItsGame::drawEnd(TFT_eSPI& tft) {
  CydFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    drawGameOver(canvas);
  });
}
