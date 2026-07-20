#include "MicriCasinoGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <math.h>

#include "../../PlayerProfile.h"
#include "../../CydFramebuffer.h"
#include "../../CydUi.h"

namespace {
Preferences casinoPrefs;
constexpr const char* CASINO_NS = "casino";
const TouchUi::GridMetrics BET_GRID = {{12, 85, 296, 40}, 8, 8, 4, 1};
const TouchUi::GridMetrics SLOT_BET_GRID = {{10, 166, 238, 30}, 6, 6, 4, 1};
constexpr TouchUi::Rect CASINO_START = {46, 158, 228, 38};
constexpr TouchUi::Rect HUB_SCENE = {12, 38, 296, 112};
constexpr TouchUi::Rect HUB_PREVIOUS = {12, 158, 54, 38};
constexpr TouchUi::Rect HUB_ENTER = {72, 158, 176, 38};
constexpr TouchUi::Rect HUB_NEXT = {254, 158, 54, 38};
constexpr TouchUi::Rect PRIMARY_ACTION = {12, 145, 190, 46};
constexpr TouchUi::Rect SECONDARY_ACTION = {212, 145, 96, 46};
constexpr TouchUi::Rect LOW_PRIMARY = {12, 174, 190, 28};
constexpr TouchUi::Rect LOW_SECONDARY = {212, 174, 96, 28};
constexpr TouchUi::Rect CONFIRM_CANCEL = {36, 145, 112, 42};
constexpr TouchUi::Rect CONFIRM_DELETE = {172, 145, 112, 42};
constexpr TouchUi::Rect HOLDEM_ACTIONS[3] = {
    {94, 163, 68, 36}, {167, 163, 68, 36}, {240, 163, 68, 36}};
constexpr TouchUi::Rect HOLDEM_AGAIN = {105, 164, 98, 34};
constexpr TouchUi::Rect HOLDEM_HUB = {210, 164, 98, 34};
constexpr TouchUi::Rect HOLDEM_RAISE_CHOICES[4] = {
    {12, 92, 68, 39}, {88, 92, 68, 39}, {164, 92, 68, 39}, {240, 92, 68, 39}};
constexpr TouchUi::Rect HOLDEM_RAISE_CONFIRM = {46, 145, 108, 42};
constexpr TouchUi::Rect HOLDEM_RAISE_CANCEL = {166, 145, 108, 42};
constexpr TouchUi::Rect VIDEO_CARDS[5] = {
    {10, 52, 44, 58}, {71, 52, 44, 58}, {132, 52, 44, 58},
    {193, 52, 44, 58}, {254, 52, 44, 58}};
constexpr TouchUi::Rect VIDEO_DRAW = {215, 145, 93, 44};
constexpr TouchUi::Rect VIDEO_PAYTABLE = {111, 145, 93, 44};
constexpr TouchUi::Rect VIDEO_PAYTABLE_CLOSE = {100, 166, 120, 30};
constexpr TouchUi::Rect VIDEO_RESULT_ACTIONS[3] = {
    {12, 174, 92, 28}, {112, 174, 92, 28}, {212, 174, 96, 28}};
constexpr TouchUi::Rect ROULETTE_TABLE_ACTION = {12, 145, 92, 46};
constexpr TouchUi::Rect ROULETTE_SPIN_ACTION = {112, 145, 92, 46};
constexpr TouchUi::Rect ROULETTE_CASINO_ACTION = {212, 145, 96, 46};
constexpr TouchUi::Rect SLOT_LEVER = {260, 65, 48, 99};
constexpr TouchUi::Rect SLOT_CASINO = {258, 171, 52, 27};
constexpr TouchUi::Rect ROULETTE_BETS[7] = {
    {12, 47, 70, 38}, {87, 47, 70, 38}, {162, 47, 70, 38}, {237, 47, 70, 38},
    {12, 91, 95, 38}, {112, 91, 95, 38}, {212, 91, 95, 38}};

constexpr int16_t TOUCH_START = 100;
constexpr int16_t TOUCH_END_RESTART = 101;
constexpr int16_t TOUCH_HUB_PREVIOUS = 110;
constexpr int16_t TOUCH_HUB_ENTER = 111;
constexpr int16_t TOUCH_HUB_NEXT = 112;

String money(Casino::Money value) {
  if (value >= 1000000UL) {
    return "$" + String(static_cast<unsigned long>(value / 1000000UL)) + "M";
  }
  if (value >= 10000UL) {
    return "$" + String(static_cast<unsigned long>(value / 1000UL)) + "K";
  }
  return "$" + String(static_cast<unsigned long>(value));
}

String signedMoney(int32_t value) {
  if (value == 0) return "$0";
  const Casino::Money magnitude = static_cast<Casino::Money>(value < 0 ? -value : value);
  return String(value > 0 ? "+" : "-") + money(magnitude);
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

bool rouletteNumberIsRed(uint8_t number) {
  static const uint8_t redNumbers[] = {
      1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36};
  for (uint8_t red : redNumbers) {
    if (red == number) return true;
  }
  return false;
}

template <typename Canvas>
void drawBetChoices(Canvas& canvas, uint8_t selected,
                    Casino::Money bankroll) {
  for (uint8_t i = 0; i < Casino::BET_COUNT; ++i) {
    const bool affordable = Casino::betAt(i) <= bankroll;
    CydUi::touchAction(canvas, BET_GRID.cardRect(i), money(Casino::betAt(i)).c_str(),
                       !affordable ? TFT_DARKGREY
                                   : (i == selected ? TFT_GOLD : TFT_LIGHTGREY),
                       i == selected);
  }
}

template <typename Canvas>
void drawSlotBetChoices(Canvas& canvas, uint8_t selected) {
  for (uint8_t i = 0; i < Casino::BET_COUNT; ++i) {
    CydUi::touchAction(canvas, SLOT_BET_GRID.cardRect(i),
                       money(Casino::slotBetAt(i)).c_str(),
                       i == selected ? TFT_GOLD : TFT_LIGHTGREY,
                       i == selected);
  }
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
  return phase() == AppPhase::Start ? 1000 : 0;
}

void MicriCasinoGame::chooseHubItem(uint8_t index) {
  if (index < 5) {
    while (logic_.hubIndex() != index) logic_.next(true);
    logic_.select(true, playerInitials());
  } else if (index == 5) {
    launchTongItsRequested_ = true;
  } else if (index == 6) {
    while (logic_.hubIndex() != 5) logic_.next(true);
    logic_.select(true, playerInitials());
  }
}

const char* MicriCasinoGame::hubLabel(uint8_t index) const {
  static const char* const labels[] = {
      "Blackjack", "Roulette", "Slots", "Holdem", "Video Poker",
      "Tong-its", "Cash Out", "Reset"};
  return labels[index % 8];
}

bool MicriCasinoGame::consumeTongItsLaunchRequest() {
  const bool requested = launchTongItsRequested_;
  launchTongItsRequested_ = false;
  return requested;
}

void MicriCasinoGame::resumeHubFromTongIts() {
  startRunning();
  hubIndex_ = 5;
}

void MicriCasinoGame::chooseBet(uint8_t index) {
  logic_.setBetIndex(index);
}

bool MicriCasinoGame::handleTouch(const TouchUi::TouchSample& sample,
                                  const TouchUi::TouchEvent& event) {
  ensureLoaded();
  const TouchUi::Point point = TouchUi::currentPoint(sample, event);
  int16_t hit = TouchUi::NO_CONTROL;
  if (phase() == AppPhase::Start) {
    if (CASINO_START.contains(point)) hit = TOUCH_START;
  } else if (phase() == AppPhase::End) {
    if (PRIMARY_ACTION.contains(point)) hit = TOUCH_END_RESTART;
  } else if (confirmReset_) {
    if (CONFIRM_CANCEL.contains(point)) hit = 90;
    else if (CONFIRM_DELETE.contains(point)) hit = 91;
  } else if (holdemRaisePicker_) {
    hit = TouchUi::hitRectList(HOLDEM_RAISE_CHOICES, 4, point);
    if (hit >= 0) hit += 80;
    else if (HOLDEM_RAISE_CONFIRM.contains(point)) hit = 84;
    else if (HOLDEM_RAISE_CANCEL.contains(point)) hit = 85;
  } else if (videoPaytable_) {
    if (VIDEO_PAYTABLE_CLOSE.contains(point)) hit = 86;
  } else {
    switch (logic_.screen()) {
      case Casino::Screen::Hub:
        if (HUB_PREVIOUS.contains(point)) hit = TOUCH_HUB_PREVIOUS;
        else if (HUB_NEXT.contains(point)) hit = TOUCH_HUB_NEXT;
        else if (HUB_ENTER.contains(point) || HUB_SCENE.contains(point)) hit = TOUCH_HUB_ENTER;
        break;
      case Casino::Screen::RouletteBet:
        hit = TouchUi::hitRectList(ROULETTE_BETS, 7, point);
        if (hit >= 0) hit += 40;
        else if (PRIMARY_ACTION.contains(point)) hit = 30;
        else if (SECONDARY_ACTION.contains(point)) hit = 31;
        break;
      case Casino::Screen::BlackjackBet:
      case Casino::Screen::HoldemBet:
      case Casino::Screen::VideoBet: {
        hit = TouchUi::cardAt(BET_GRID, Casino::BET_COUNT, 0, point);
        if (hit >= 0) hit += 20;
        else if (PRIMARY_ACTION.contains(point)) hit = 30;
        else if (SECONDARY_ACTION.contains(point)) hit = 31;
        break;
      }
      case Casino::Screen::RouletteStake:
        hit = TouchUi::cardAt(BET_GRID, Casino::BET_COUNT, 0, point);
        if (hit >= 0) hit += 20;
        else if (ROULETTE_TABLE_ACTION.contains(point)) hit = 67;
        else if (ROULETTE_SPIN_ACTION.contains(point)) hit = 68;
        else if (ROULETTE_CASINO_ACTION.contains(point)) hit = 31;
        break;
      case Casino::Screen::SlotsIdle:
        hit = TouchUi::cardAt(SLOT_BET_GRID, Casino::BET_COUNT, 0, point);
        if (hit >= 0) hit += 20;
        else if (SLOT_LEVER.contains(point)) hit = 30;
        else if (SLOT_CASINO.contains(point)) hit = 31;
        break;
      case Casino::Screen::BlackjackTurn:
        if (LOW_PRIMARY.contains(point)) hit = 32;
        else if (LOW_SECONDARY.contains(point)) hit = 33;
        break;
      case Casino::Screen::HoldemAction:
        if (logic_.holdemActor() == 0 && !logic_.holdemPlayerFolded()) {
          hit = TouchUi::hitRectList(HOLDEM_ACTIONS, 3, point);
          if (hit >= 0) hit += 50;
        }
        break;
      case Casino::Screen::VideoHold:
        if (!logic_.videoAnimating()) {
          hit = TouchUi::hitRectList(VIDEO_CARDS, 5, point);
          if (hit >= 0) hit += 60;
          else if (VIDEO_DRAW.contains(point)) hit = 65;
          else if (VIDEO_PAYTABLE.contains(point)) hit = 66;
        }
        break;
      case Casino::Screen::HoldemResult:
        if (HOLDEM_AGAIN.contains(point)) hit = 34;
        else if (HOLDEM_HUB.contains(point)) hit = 35;
        break;
      case Casino::Screen::BlackjackResult:
      case Casino::Screen::RouletteResult:
        if (LOW_PRIMARY.contains(point)) hit = 34;
        else if (LOW_SECONDARY.contains(point)) hit = 35;
        break;
      case Casino::Screen::VideoResult:
        hit = TouchUi::hitRectList(VIDEO_RESULT_ACTIONS, 3, point);
        if (hit >= 0) hit += 87;
        break;
      case Casino::Screen::SlotsResult:
        if (SLOT_LEVER.contains(point)) hit = 34;
        else if (SLOT_CASINO.contains(point)) hit = 35;
        break;
      case Casino::Screen::Broke:
        if (PRIMARY_ACTION.contains(point)) hit = 36;
        break;
      case Casino::Screen::CashOut:
        if (PRIMARY_ACTION.contains(point)) hit = 37;
        break;
      default:
        return sample.down || event.released;
    }
  }

  const TouchUi::CaptureResult result = touchCapture_.update(sample, event, hit);
  if (!result.activated) return result.consumed;
  const int16_t id = result.id;
  if (id == TOUCH_START || id == TOUCH_END_RESTART) {
    startRunning();
  } else if (confirmReset_) {
    if (id == 91) logic_.resetBankroll();
    confirmReset_ = false;
  } else if (holdemRaisePicker_) {
    if (id >= 80 && id < 84) {
      holdemRaiseIndex_ = static_cast<uint8_t>(id - 80);
    } else if (id == 84) {
      logic_.setHoldemRaiseAmount(holdemRaiseChoice(holdemRaiseIndex_));
      logic_.setHoldemAction(Casino::HoldemAction::Raise);
      logic_.select(true, playerInitials());
      holdemRaisePicker_ = false;
    } else if (id == 85) {
      holdemRaisePicker_ = false;
    }
  } else if (videoPaytable_) {
    if (id == 86) videoPaytable_ = false;
  } else if (logic_.screen() == Casino::Screen::Hub && id == TOUCH_HUB_PREVIOUS) {
    hubIndex_ = hubIndex_ == 0 ? 7 : hubIndex_ - 1;
  } else if (logic_.screen() == Casino::Screen::Hub && id == TOUCH_HUB_NEXT) {
    hubIndex_ = static_cast<uint8_t>((hubIndex_ + 1) % 8);
  } else if (logic_.screen() == Casino::Screen::Hub && id == TOUCH_HUB_ENTER) {
    if (hubIndex_ == 7) confirmReset_ = true;
    else chooseHubItem(hubIndex_);
  } else if (id >= 20 && id < 20 + Casino::BET_COUNT) {
    chooseBet(static_cast<uint8_t>(id - 20));
  } else if (id >= 40 && id < 47) {
    while (static_cast<uint8_t>(logic_.rouletteBet()) != id - 40) logic_.next(true);
  } else if (id == 67) {
    logic_.commitRouletteBet(false, playerInitials());
  } else if (id == 68) {
    logic_.commitRouletteBet(true, playerInitials());
  } else if (id == 30) {
    if (logic_.screen() == Casino::Screen::BlackjackBet) {
      logic_.next(true);
    } else {
      logic_.select(true, playerInitials());
    }
  } else if (id == 31) {
    logic_.previous();
  } else if (id == 32) {
    logic_.next(true);
  } else if (id == 33) {
    logic_.select(true, playerInitials());
  } else if (id >= 50 && id < 53) {
    const Casino::HoldemAction action = static_cast<Casino::HoldemAction>(id - 50);
    if (action == Casino::HoldemAction::Raise) {
      if (logic_.holdemCanRaise()) {
        holdemRaiseIndex_ = 0;
        holdemRaisePicker_ = true;
      }
    } else {
      logic_.setHoldemAction(action);
      logic_.select(true, playerInitials());
    }
  } else if (id >= 60 && id < 65) {
    while (logic_.videoCursor() != id - 60) logic_.next(true);
    logic_.select(true, playerInitials());
  } else if (id == 65) {
    while (logic_.videoCursor() != 5) logic_.next(true);
    logic_.select(true, playerInitials());
  } else if (id == 66) {
    videoPaytable_ = true;
  } else if (id == 87) {
    logic_.next(true);
  } else if (id == 88) {
    logic_.select(true, playerInitials());
  } else if (id == 89) {
    logic_.previous();
  } else if (id == 34) {
    if (logic_.screen() == Casino::Screen::SlotsResult) {
      logic_.next(true);
      logic_.select(true, playerInitials());
    } else {
      logic_.next(true);
    }
  } else if (id == 36 || id == 37) {
    logic_.next(true);
  } else if (id == 35) {
    logic_.previous();
  }
  saveBankroll();
  return true;
}

void MicriCasinoGame::ensureLoaded() {
  if (loaded_) {
    return;
  }
  casinoPrefs.begin(CASINO_NS, true);
  const Casino::Money bank = casinoPrefs.getUInt("bank", Casino::START_BANKROLL);
  const Casino::Money best = casinoPrefs.getUInt("cashout", 0);
  const uint16_t initials = casinoPrefs.getUShort("cashwho", PlayerProfile::defaultInitials());
  casinoPrefs.end();
  logic_.seed(micros() ^ millis());
  logic_.loadBankroll(bank, best, initials);
  savedBankroll_ = logic_.bankroll();
  savedBestCashout_ = logic_.bestCashout();
  savedBestCashoutInitials_ = logic_.bestCashoutInitials();
  loaded_ = true;
}

void MicriCasinoGame::saveBankroll() {
  if (!loaded_) {
    return;
  }
  if (savedBankroll_ == logic_.bankroll() &&
      savedBestCashout_ == logic_.bestCashout() &&
      savedBestCashoutInitials_ == logic_.bestCashoutInitials()) {
    return;
  }
  casinoPrefs.begin(CASINO_NS, false);
  casinoPrefs.putUInt("bank", logic_.bankroll());
  casinoPrefs.putUInt("cashout", logic_.bestCashout());
  casinoPrefs.putUShort("cashwho", logic_.bestCashoutInitials());
  casinoPrefs.end();
  savedBankroll_ = logic_.bankroll();
  savedBestCashout_ = logic_.bestCashout();
  savedBestCashoutInitials_ = logic_.bestCashoutInitials();
}

uint16_t MicriCasinoGame::playerInitials() const {
  return PlayerProfile::loadInitials();
}

Casino::Money MicriCasinoGame::holdemRaiseChoice(uint8_t index) const {
  static const uint8_t multipliers[4] = {1, 2, 5, 10};
  return logic_.holdemBigBlind() * multipliers[index % 4];
}

void MicriCasinoGame::onAppReset() {
  ensureLoaded();
  logic_.resetToHub();
  confirmReset_ = false;
  launchTongItsRequested_ = false;
  holdemRaisePicker_ = false;
  holdemRaiseIndex_ = 0;
  videoPaytable_ = false;
  hubIndex_ = 0;
  touchCapture_.reset();
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
    videoPaytable_ = false;
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
  canvas.fillRect(0, 0, CydUi::W, CydUi::HEADER_H, TFT_BLACK);
  canvas.drawFastHLine(0, CydUi::HEADER_H - 1, CydUi::W, TFT_DARKGREY);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(2);
  canvas.setTextColor(TFT_GOLD, TFT_BLACK);
  canvas.drawString(CydUi::fitText(canvas, title, 178), CydUi::PAD, 9);

  const bool showGame = logic_.screen() != Casino::Screen::Hub &&
                        logic_.screen() != Casino::Screen::CashOut &&
                        logic_.screen() != Casino::Screen::Broke;
  canvas.setTextSize(1);
  const String wallet = "Wallet " + money(logic_.bankroll());
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString(wallet, CydUi::W - CydUi::PAD - canvas.textWidth(wallet),
                    showGame ? 4 : 14);
  if (showGame) {
    const int32_t net = logic_.gameProfitLoss();
    const String game = "Game " + signedMoney(net);
    canvas.setTextColor(net > 0 ? TFT_GREEN : (net < 0 ? TFT_RED : TFT_YELLOW),
                        TFT_BLACK);
    canvas.drawString(game, CydUi::W - CydUi::PAD - canvas.textWidth(game), 19);
  }
}

template <typename Canvas>
void MicriCasinoGame::drawCard(Canvas& canvas, int x, int y, uint8_t card, bool hidden) {
  canvas.fillRoundRect(x, y, 36, 46, 4, hidden ? TFT_DARKGREY : TFT_WHITE);
  canvas.drawRoundRect(x, y, 36, 46, 4, TFT_LIGHTGREY);
  canvas.setTextDatum(TL_DATUM);
  if (hidden) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_BLACK, TFT_DARKGREY);
    canvas.setTextSize(2);
    canvas.drawString("?", x + 13, y + 15);
    return;
  }
  const char* label = Casino::rankChar(card) == 'T' ? "10" : nullptr;
  char shortLabel[2] = {Casino::rankChar(card), '\0'};
  if (label == nullptr) label = shortLabel;
  const uint8_t suit = Casino::cardSuit(card);
  const uint16_t color = suit == 1 || suit == 2 ? TFT_RED : TFT_BLACK;
  canvas.setTextSize(label[1] == '\0' ? 3 : 2);
  canvas.setTextColor(color, TFT_WHITE);
  canvas.drawString(label, x + (label[1] == '\0' ? 9 : 6),
                    y + (label[1] == '\0' ? 4 : 8));
  drawSuit(canvas, x + 18, y + 34, suit);
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
  CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    drawShell(canvas, "Micri Casino");
    if (showStartScorePage()) {
      char dotted[4];
      PlayerProfile::unpackDottedInitials(logic_.bestCashoutInitials(), dotted);
      CydUi::centered(canvas, "Best Cashout", 47, 2, TFT_LIGHTGREY);
      CydUi::largeValue(canvas, money(logic_.bestCashout()), 70, TFT_GREEN);
      CydUi::centered(canvas, dotted, 124, 2, TFT_WHITE);
    } else if (showStartPromptPage()) {
      CydUi::largeValue(canvas, "PLAY", 54, TFT_GREEN);
      CydUi::centered(canvas, "Start with $200 credits", 124, 2, TFT_LIGHTGREY);
    } else {
      drawCard(canvas, 28, 63, 0);
      drawCard(canvas, 72, 54, 12);
      canvas.drawRoundRect(133, 57, 62, 76, 6, TFT_GOLD);
      canvas.fillRect(143, 68, 42, 11, TFT_RED);
      canvas.fillRoundRect(143, 92, 11, 11, 2, TFT_YELLOW);
      canvas.fillRoundRect(159, 92, 11, 11, 2, TFT_GREEN);
      canvas.fillRoundRect(175, 92, 11, 11, 2, TFT_CYAN);
      canvas.drawCircle(249, 74, 19, TFT_RED);
      canvas.drawCircle(273, 95, 19, TFT_BLUE);
      canvas.drawCircle(229, 112, 19, TFT_GREEN);
      CydUi::centered(canvas, "fictional credits only", 132, 1, TFT_LIGHTGREY);
    }
    CydUi::touchAction(canvas, CASINO_START, "Enter Casino", TFT_GREEN);
  });
}

void MicriCasinoGame::drawRunning(TFT_eSPI& tft) {
  ensureLoaded();
  CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
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
    if (confirmReset_) {
      CydUi::confirmation(canvas, "Reset Casino", "Restore bankroll to $200?",
                          CONFIRM_CANCEL, CONFIRM_DELETE, "Reset");
    }
    if (holdemRaisePicker_) drawHoldemRaisePicker(canvas);
    if (videoPaytable_) drawVideoPaytable(canvas);
  });
  logic_.clearDirty();
}

template <typename Canvas>
void MicriCasinoGame::drawHub(Canvas& canvas) {
  drawShell(canvas, "Micri Casino");
  drawHubScene(canvas, hubIndex_);
  CydUi::touchAction(canvas, HUB_PREVIOUS, "<", TFT_CYAN, false);
  const char* action = hubIndex_ <= 5 ? "Play" : hubLabel(hubIndex_);
  const uint16_t actionColor = hubIndex_ == 7 ? TFT_RED : TFT_GREEN;
  CydUi::touchAction(canvas, HUB_ENTER, action, actionColor);
  CydUi::touchAction(canvas, HUB_NEXT, ">", TFT_CYAN, false);
}

template <typename Canvas>
void MicriCasinoGame::drawHubScene(Canvas& canvas, uint8_t index) {
  const int16_t x = HUB_SCENE.x;
  const int16_t y = HUB_SCENE.y;
  const int16_t w = HUB_SCENE.w;
  const int16_t h = HUB_SCENE.h;
  const uint16_t panel = index == 7 ? TFT_MAROON : TFT_NAVY;
  canvas.fillRoundRect(x, y, w, h, 8, panel);
  canvas.drawRoundRect(x, y, w, h, 8, index == 7 ? TFT_RED : TFT_GOLD);

  if (index == 0) {
    canvas.fillEllipse(160, 86, 100, 38, TFT_DARKGREEN);
    canvas.drawEllipse(160, 86, 100, 38, TFT_GREEN);
    drawCard(canvas, 130, 54, 0);
    drawCard(canvas, 169, 54, 12);
    canvas.fillCircle(87, 84, 12, TFT_RED);
    canvas.drawCircle(87, 84, 8, TFT_WHITE);
    canvas.fillCircle(235, 84, 12, TFT_BLUE);
    canvas.drawCircle(235, 84, 8, TFT_WHITE);
  } else if (index == 1) {
    canvas.fillCircle(160, 81, 42, TFT_DARKGREEN);
    canvas.drawCircle(160, 81, 42, TFT_GOLD);
    canvas.drawCircle(160, 81, 31, TFT_RED);
    canvas.drawCircle(160, 81, 17, TFT_WHITE);
    for (uint8_t i = 0; i < 12; ++i) {
      const float angle = i * 0.523599f;
      canvas.drawLine(160, 81, 160 + cosf(angle) * 40, 81 + sinf(angle) * 40,
                      (i & 1) ? TFT_WHITE : TFT_RED);
    }
    canvas.fillCircle(185, 58, 4, TFT_WHITE);
  } else if (index == 2) {
    canvas.fillTriangle(40, 119, 280, 119, 160, 69, TFT_DARKGREY);
    canvas.drawLine(160, 69, 80, 119, TFT_MAGENTA);
    canvas.drawLine(160, 69, 240, 119, TFT_CYAN);
    for (uint8_t i = 0; i < 4; ++i) {
      const int16_t mx = 47 + i * 62;
      canvas.fillRoundRect(mx, 50, 42, 64, 5, TFT_DARKGREY);
      canvas.drawRoundRect(mx, 50, 42, 64, 5, TFT_GOLD);
      canvas.fillRect(mx + 6, 58, 30, 27, i & 1 ? TFT_PURPLE : TFT_BLUE);
      canvas.setTextSize(2);
      canvas.setTextColor(TFT_YELLOW, i & 1 ? TFT_PURPLE : TFT_BLUE);
      canvas.drawString("7", mx + 15, 63);
      canvas.fillCircle(mx + 21, 100, 5, TFT_RED);
    }
  } else if (index == 3) {
    canvas.fillEllipse(160, 82, 112, 42, TFT_DARKGREEN);
    canvas.drawEllipse(160, 82, 112, 42, TFT_GOLD);
    drawCard(canvas, 137, 52, 9);
    drawCard(canvas, 176, 52, 24);
    canvas.fillCircle(91, 91, 11, TFT_RED);
    canvas.fillCircle(229, 91, 11, TFT_BLUE);
  } else if (index == 4) {
    canvas.fillRoundRect(68, 48, 184, 72, 7, TFT_DARKGREY);
    canvas.drawRoundRect(68, 48, 184, 72, 7, TFT_CYAN);
    canvas.fillRect(78, 56, 164, 45, TFT_DARKGREEN);
    for (uint8_t i = 0; i < 5; ++i) {
      canvas.fillRoundRect(86 + i * 31, 62, 24, 33, 2, TFT_WHITE);
      canvas.drawRoundRect(86 + i * 31, 62, 24, 33, 2, TFT_LIGHTGREY);
      canvas.fillCircle(98 + i * 31, 78, 4, i & 1 ? TFT_RED : TFT_BLACK);
    }
    canvas.fillRoundRect(132, 106, 56, 8, 3, TFT_GOLD);
  } else if (index == 5) {
    canvas.fillEllipse(160, 82, 112, 42, TFT_DARKGREEN);
    canvas.drawEllipse(160, 82, 112, 42, TFT_GOLD);
    for (uint8_t i = 0; i < 3; ++i) {
      drawCard(canvas, 112 + i * 40, 52, static_cast<uint8_t>(2 + i * 13));
    }
    canvas.fillCircle(75, 83, 10, TFT_MAGENTA);
    canvas.fillCircle(246, 83, 10, TFT_CYAN);
  } else if (index == 6) {
    canvas.fillRoundRect(62, 51, 196, 65, 7, TFT_DARKGREY);
    canvas.drawRoundRect(62, 51, 196, 65, 7, TFT_CYAN);
    canvas.fillRect(74, 62, 92, 42, TFT_BLACK);
    canvas.fillCircle(112, 83, 14, TFT_GOLD);
    canvas.drawCircle(112, 83, 9, TFT_WHITE);
    canvas.fillTriangle(182, 73, 182, 94, 210, 83, TFT_GREEN);
    canvas.fillRect(207, 71, 35, 25, TFT_DARKGREEN);
  } else {
    canvas.drawCircle(160, 80, 31, TFT_RED);
    canvas.drawCircle(160, 80, 22, TFT_ORANGE);
    canvas.setTextDatum(MC_DATUM);
    canvas.setTextSize(3);
    canvas.setTextColor(TFT_WHITE, panel);
    canvas.drawString("!", 160, 80);
    canvas.setTextDatum(TL_DATUM);
  }

  canvas.fillRect(x + 1, y + h - 27, w - 2, 26, TFT_BLACK);
  CydUi::centered(canvas, hubLabel(index), y + h - 23, 2,
                  index == 7 ? TFT_RED : TFT_WHITE);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  const String page = String(index + 1) + "/8";
  canvas.drawRightString(page, x + w - 8, y + h - 19, 1);
}

template <typename Canvas>
void MicriCasinoGame::drawBlackjack(Canvas& canvas) {
  drawShell(canvas, "Blackjack");
  if (logic_.screen() == Casino::Screen::BlackjackShuffling) {
    canvas.fillRoundRect(105, 57, 45, 64, 5, TFT_NAVY);
    canvas.drawRoundRect(105, 57, 45, 64, 5, TFT_WHITE);
    canvas.fillRoundRect(170, 57, 45, 64, 5, TFT_DARKGREEN);
    canvas.drawRoundRect(170, 57, 45, 64, 5, TFT_WHITE);
    CydUi::centered(canvas, "Shuffling", 138, 2, TFT_YELLOW);
    CydUi::footer(canvas, "Two-deck shoe");
    return;
  }

  if (logic_.screen() == Casino::Screen::BlackjackBet) {
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("Bet", 28, 61);
    drawBetChoices(canvas, logic_.betIndex(), logic_.bankroll());
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString(String(logic_.blackjackDeckRemaining()) + " cards left", 12, 132);
    CydUi::touchAction(canvas, PRIMARY_ACTION, "Deal", TFT_GREEN);
    CydUi::touchAction(canvas, SECONDARY_ACTION, "Casino", TFT_LIGHTGREY, false);
    return;
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas.drawString("DEALER", 10, 43);
  for (uint8_t i = 0; i < logic_.dealerCount() && i < 5; i++) {
    drawCard(canvas, 98 + i * 40, 41, logic_.dealerCards()[i], logic_.blackjackHideDealer() && i == 0);
  }
  if (!logic_.blackjackHideDealer() && logic_.dealerCount() > 0) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
    canvas.drawString(String(logic_.blackjackDealerValue()), 76, 43);
  }
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_GREEN, TFT_BLACK);
  canvas.drawString("PLAYER", 10, 108);
  for (uint8_t i = 0; i < logic_.playerCount() && i < 5; i++) {
    drawCard(canvas, 98 + i * 40, 106, logic_.playerCards()[i]);
  }
  if (logic_.playerCount() > 0) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
    canvas.drawString(String(logic_.blackjackPlayerValue()), 76, 108);
  }
  if (logic_.screen() == Casino::Screen::BlackjackTurn) {
    CydUi::touchAction(canvas, LOW_PRIMARY, "Hit", TFT_GREEN);
    CydUi::touchAction(canvas, LOW_SECONDARY, "Stand", TFT_ORANGE);
  } else if (logic_.screen() == Casino::Screen::BlackjackResult) {
    const bool won = logic_.outcome() == Casino::Outcome::Win ||
                     logic_.outcome() == Casino::Outcome::Blackjack;
    const uint16_t resultColor = won ? TFT_GREEN
                                     : (logic_.outcome() == Casino::Outcome::Push ? TFT_YELLOW : TFT_RED);
    CydUi::pill(canvas, 10, 148, Casino::blackjackResultName(logic_.outcome()), resultColor);
    CydUi::touchAction(canvas, LOW_PRIMARY, "Play Again", TFT_GREEN);
    CydUi::touchAction(canvas, LOW_SECONDARY, "Casino", TFT_LIGHTGREY, false);
  } else {
    CydUi::centered(canvas, logic_.blackjackFooterText(), 184, 1, TFT_LIGHTGREY);
  }
}

template <typename Canvas>
void MicriCasinoGame::drawRoulette(Canvas& canvas) {
  drawShell(canvas, "Roulette");
  if (logic_.screen() == Casino::Screen::RouletteBet) {
    canvas.fillRoundRect(7, 41, 306, 94, 5, TFT_DARKGREEN);
    canvas.drawRoundRect(7, 41, 306, 94, 5, TFT_GREEN);
    for (uint8_t i = 0; i < 7; ++i) {
      const TouchUi::Rect& rect = ROULETTE_BETS[i];
      const bool selected = i == static_cast<uint8_t>(logic_.rouletteBet());
      const uint16_t bg = i == 0 ? TFT_RED : (i == 1 ? TFT_BLACK : TFT_DARKGREEN);
      canvas.fillRect(rect.x, rect.y, rect.w, rect.h, bg);
      canvas.drawRect(rect.x, rect.y, rect.w, rect.h, selected ? TFT_GOLD : TFT_WHITE);
      if (selected) canvas.drawRect(rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4, TFT_GOLD);
      const Casino::Money wager = logic_.rouletteWager(static_cast<Casino::RouletteBet>(i));
      canvas.setTextSize(2);
      canvas.setTextColor(TFT_WHITE, bg);
      const String label = Casino::rouletteBetName(static_cast<Casino::RouletteBet>(i));
      canvas.drawString(label, rect.x + (rect.w - canvas.textWidth(label)) / 2,
                        rect.y + (wager > 0 ? 4 : 11));
      if (wager > 0) {
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_GOLD, bg);
        const String amount = money(wager);
        canvas.drawString(amount, rect.x + (rect.w - canvas.textWidth(amount)) / 2,
                          rect.y + 25);
      }
      if (selected) canvas.fillCircle(rect.x + rect.w - 9, rect.y + 9, 5, TFT_GOLD);
    }
    CydUi::centered(canvas, "On table " + money(logic_.rouletteTotalWager()), 132, 1, TFT_GOLD);
    CydUi::touchAction(canvas, PRIMARY_ACTION, "Set Stake", TFT_GREEN);
    CydUi::touchAction(canvas, SECONDARY_ACTION, "Casino", TFT_LIGHTGREY, false);
    return;
  }
  if (logic_.screen() == Casino::Screen::RouletteStake) {
    CydUi::centered(canvas, Casino::rouletteBetName(logic_.rouletteBet()), 45, 2, TFT_WHITE);
    drawBetChoices(canvas, logic_.betIndex(), logic_.rouletteStakeBudget());
    CydUi::touchAction(canvas, ROULETTE_TABLE_ACTION, "Table", TFT_CYAN, false);
    CydUi::touchAction(canvas, ROULETTE_SPIN_ACTION, "Spin", TFT_GREEN);
    CydUi::touchAction(canvas, ROULETTE_CASINO_ACTION, "Casino", TFT_LIGHTGREY, false);
    return;
  }
  const uint8_t displayNumber = logic_.screen() == Casino::Screen::RouletteSpin ? logic_.rouletteDisplayNumber() : logic_.rouletteNumber();
  const bool red = rouletteNumberIsRed(displayNumber);
  const uint16_t color = displayNumber == 0 ? TFT_GREEN : (red ? TFT_RED : TFT_DARKGREY);
  if (logic_.screen() == Casino::Screen::RouletteSpin) {
    static const uint8_t wheelNumbers[37] = {
        0, 32, 15, 19, 4, 21, 2, 25, 17, 34, 6, 27, 13, 36, 11, 30, 8, 23,
        10, 5, 24, 16, 33, 1, 20, 14, 31, 9, 22, 18, 29, 7, 28, 12, 35, 3, 26};
    const float step = 6.2831853f / 37.0f;
    const float rotation = logic_.rouletteSpinMs() * 0.0065f;
    for (uint8_t i = 0; i < 37; ++i) {
      const float a0 = rotation + i * step;
      const float a1 = a0 + step * 0.94f;
      const uint8_t pocket = wheelNumbers[i];
      const uint16_t pocketColor = pocket == 0
                                       ? TFT_GREEN
                                       : (rouletteNumberIsRed(pocket) ? TFT_RED : TFT_DARKGREY);
      canvas.fillTriangle(95, 108,
                          95 + static_cast<int16_t>(cos(a0) * 55),
                          108 + static_cast<int16_t>(sin(a0) * 55),
                          95 + static_cast<int16_t>(cos(a1) * 55),
                          108 + static_cast<int16_t>(sin(a1) * 55),
                          pocketColor);
    }
    canvas.fillCircle(95, 108, 30, TFT_BLACK);
  } else {
    canvas.fillCircle(95, 108, 55, color);
  }
  canvas.drawCircle(95, 108, 59, TFT_WHITE);
  if (logic_.screen() == Casino::Screen::RouletteSpin) {
    const float angle = (logic_.rouletteSpinMs() * 0.024f);
    canvas.fillCircle(95 + cos(angle) * 66, 108 + sin(angle) * 66, 5, TFT_WHITE);
  }
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextSize(3);
  canvas.setTextColor(TFT_WHITE,
                      logic_.screen() == Casino::Screen::RouletteSpin ? TFT_BLACK : color);
  canvas.drawString(String(displayNumber), 95, 108);
  canvas.setTextDatum(TL_DATUM);
  if (logic_.screen() == Casino::Screen::RouletteSpin) {
    CydUi::pill(canvas, 190, 68, "SPIN", TFT_YELLOW);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("Ball is slowing...", 190, 105);
    return;
  }
  const bool won = logic_.outcome() == Casino::Outcome::Win;
  const uint16_t resultColor = won ? TFT_GREEN
                                   : (logic_.outcome() == Casino::Outcome::Push ? TFT_YELLOW : TFT_RED);
  CydUi::pill(canvas, 190, 68, Casino::outcomeName(logic_.outcome()), resultColor);
  canvas.setTextSize(2);
  canvas.setTextColor(TFT_GOLD, TFT_BLACK);
  canvas.drawString("Paid " + money(logic_.lastWin()), 190, 105);
  CydUi::touchAction(canvas, LOW_PRIMARY, "Bet Again", TFT_GREEN);
  CydUi::touchAction(canvas, LOW_SECONDARY, "Casino", TFT_LIGHTGREY, false);
}

template <typename Canvas>
void MicriCasinoGame::drawSlots(Canvas& canvas) {
  drawShell(canvas, "Slots");
  const bool idle = logic_.screen() == Casino::Screen::SlotsIdle;
  const int startX = 14;
  const int startY = 50;
  canvas.fillRoundRect(6, 40, 247, 123, 8, TFT_DARKGREY);
  canvas.drawRoundRect(6, 40, 247, 123, 8, TFT_GOLD);
  canvas.fillRect(11, 45, 237, 112, TFT_BLACK);
  for (uint8_t x = 0; x < 5; x++) {
    for (uint8_t y = 0; y < 3; y++) {
      const uint8_t symbol = logic_.slotDisplaySymbol(x, y);
      const int px = startX + x * 46;
      const int py = startY + y * 35;
      canvas.fillRoundRect(px, py, 40, 30, 4, slotColor(symbol));
      canvas.setTextSize(3);
      canvas.setTextColor(TFT_BLACK, slotColor(symbol));
      char label[2] = {slotChar(symbol), '\0'};
      canvas.drawString(label, px + 12, py + 4);
      if (!logic_.slotsSpinning() && logic_.lastWin() > 0 && logic_.slotWinCell(x, y)) {
        const uint16_t border = ((millis() / 180) % 2) == 0 ? TFT_WHITE : TFT_GOLD;
        canvas.drawRoundRect(px - 2, py - 2, 44, 34, 5, border);
        canvas.drawRoundRect(px - 3, py - 3, 46, 36, 6, border);
      }
    }
  }
  const int knobY = logic_.slotsSpinning() ? 142 : 69;
  canvas.drawFastVLine(284, 79, 64, TFT_LIGHTGREY);
  canvas.drawLine(284, 79, 284, knobY, TFT_WHITE);
  canvas.fillCircle(284, knobY, 11, logic_.slotsSpinning() ? TFT_YELLOW : TFT_RED);
  canvas.drawCircle(284, knobY, 11, TFT_WHITE);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString("PULL", 269, 151);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  if (idle) {
    drawSlotBetChoices(canvas, logic_.betIndex());
    CydUi::touchAction(canvas, SLOT_CASINO, "Back", TFT_LIGHTGREY, false);
  } else if (logic_.slotsSpinning()) {
    CydUi::pill(canvas, 262, 174, "SPIN", TFT_YELLOW);
  } else {
    canvas.drawString("Bet " + money(logic_.slotBet()) + "  Free " + String(logic_.freeSpins()), 12, 185);
    if (logic_.lastWin() > 0) {
      CydUi::pill(canvas, 137, 174, ("Win " + money(logic_.lastWin())).c_str(), TFT_GREEN);
    }
    CydUi::touchAction(canvas, SLOT_CASINO, "Back", TFT_LIGHTGREY, false);
  }
}

template <typename Canvas>
void MicriCasinoGame::drawHoldem(Canvas& canvas) {
  drawShell(canvas, "Holdem");
  if (logic_.screen() == Casino::Screen::HoldemBet) {
    CydUi::centered(canvas, "Choose table big blind", 48, 2, TFT_WHITE);
    drawBetChoices(canvas, logic_.betIndex(), logic_.bankroll());
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("Table max: " + money(logic_.bet() * 10), 12, 132);
    CydUi::touchAction(canvas, PRIMARY_ACTION, "Join Table", TFT_GREEN);
    CydUi::touchAction(canvas, SECONDARY_ACTION, "Casino", TFT_LIGHTGREY, false);
    return;
  }
  canvas.fillRect(0, 29, width, height - CydUi::FOOTER_H - 29, TFT_DARKGREEN);
  canvas.drawFastHLine(0, 29, width, TFT_GREEN);
  canvas.drawFastHLine(0, height - CydUi::FOOTER_H - 1, width, TFT_GREEN);

  const int16_t seatX[3] = {8, 112, 216};
  for (uint8_t seat = 1; seat < Casino::Logic::HOLDEM_SEATS; ++seat) {
    const int16_t x = seatX[seat - 1];
    const bool actor = logic_.screen() == Casino::Screen::HoldemAction &&
                       logic_.holdemActor() == seat;
    const bool folded = logic_.holdemSeatFolded(seat);
    canvas.setTextSize(1);
    canvas.setTextColor(folded ? TFT_LIGHTGREY : (actor ? TFT_YELLOW : TFT_WHITE),
                        TFT_DARKGREEN);
    canvas.drawString(String(logic_.holdemSeatName(seat)) + (folded ? " FOLD" : ""), x, 34);
    const bool hide = logic_.screen() == Casino::Screen::HoldemAction || folded;
    drawCard(canvas, x, 45, logic_.holdemSeatCards(seat)[0], hide);
    drawCard(canvas, x + 39, 45, logic_.holdemSeatCards(seat)[1], hide);
    if (actor) canvas.drawRoundRect(x - 3, 42, 81, 52, 5, TFT_YELLOW);
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  canvas.drawString(holdemStreetLabel(logic_.communityShown()), 8, 103);
  canvas.setTextColor(TFT_GOLD, TFT_DARKGREEN);
  canvas.drawRightString("Pot " + money(logic_.pot()), 311, 103, 1);
  for (uint8_t i = 0; i < logic_.communityShown(); i++) {
    drawCard(canvas, 59 + i * 40, 100, logic_.holdemCommunity()[i]);
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  canvas.drawString(logic_.holdemPlayerFolded() ? "You FOLD" : "You", 8, 153);
  drawCard(canvas, 8, 160, logic_.holdemPlayer()[0]);
  drawCard(canvas, 47, 160, logic_.holdemPlayer()[1]);

  if (logic_.screen() == Casino::Screen::HoldemAction) {
    if (logic_.holdemActor() == 0 && !logic_.holdemPlayerFolded()) {
      String primary = logic_.holdemPrimaryActionName();
      if (logic_.holdemCallAmount() > 0) primary += " " + money(logic_.holdemCallAmount());
      CydUi::touchAction(canvas, HOLDEM_ACTIONS[0], primary.c_str(), TFT_GREEN);
      CydUi::touchAction(canvas, HOLDEM_ACTIONS[1], "Raise",
                         logic_.holdemCanRaise() ? TFT_GOLD : TFT_DARKGREY,
                         logic_.holdemCanRaise());
      CydUi::touchAction(canvas, HOLDEM_ACTIONS[2], "Fold", TFT_RED);
    } else {
      CydUi::pill(canvas, 107, 166, logic_.holdemMessage(), TFT_YELLOW);
    }
  } else {
    CydUi::touchAction(canvas, HOLDEM_AGAIN, "Play Again", TFT_GREEN);
    CydUi::touchAction(canvas, HOLDEM_HUB, "Casino", TFT_LIGHTGREY, false);
    const uint16_t resultColor = logic_.outcome() == Casino::Outcome::Lose ? TFT_RED : TFT_GREEN;
    CydUi::pill(canvas, 88, 143, Casino::outcomeName(logic_.outcome()), resultColor);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    const uint8_t winner = logic_.holdemWinnerSeat();
    const String result = logic_.holdemWinningScore() == 0
                              ? String(logic_.holdemMessage())
                              : (winner < Casino::Logic::HOLDEM_SEATS
                                     ? String(logic_.holdemSeatName(winner)) + " - " +
                                           Casino::pokerScoreName(logic_.holdemWinningScore())
                                     : String(logic_.holdemMessage()));
    canvas.drawString(result, 145, 148);
  }
}

template <typename Canvas>
void MicriCasinoGame::drawHoldemRaisePicker(Canvas& canvas) {
  canvas.fillRoundRect(6, 37, 308, 160, 8, TFT_NAVY);
  canvas.drawRoundRect(6, 37, 308, 160, 8, TFT_GOLD);
  CydUi::centered(canvas, "Raise amount", 50, 2, TFT_WHITE);
  for (uint8_t i = 0; i < 4; ++i) {
    CydUi::touchAction(canvas, HOLDEM_RAISE_CHOICES[i],
                       money(holdemRaiseChoice(i)).c_str(),
                       i == holdemRaiseIndex_ ? TFT_GOLD : TFT_LIGHTGREY,
                       i == holdemRaiseIndex_);
  }
  CydUi::touchAction(canvas, HOLDEM_RAISE_CONFIRM, "Raise", TFT_GREEN);
  CydUi::touchAction(canvas, HOLDEM_RAISE_CANCEL, "Cancel", TFT_LIGHTGREY, false);
}

template <typename Canvas>
void MicriCasinoGame::drawVideoPoker(Canvas& canvas) {
  drawShell(canvas, "Video Poker");
  if (logic_.screen() == Casino::Screen::VideoBet) {
    CydUi::centered(canvas, "Jacks or Better", 48, 2, TFT_LIGHTGREY);
    drawBetChoices(canvas, logic_.betIndex(), logic_.bankroll());
    CydUi::touchAction(canvas, PRIMARY_ACTION, "Deal", TFT_GREEN);
    CydUi::touchAction(canvas, SECONDARY_ACTION, "Casino", TFT_LIGHTGREY, false);
    return;
  }
  for (uint8_t i = 0; i < 5; i++) {
    drawCard(canvas, 13 + i * 61, 56, logic_.videoDisplayCard(i));
    if (logic_.videoHeld(i)) {
      CydUi::pill(canvas, 14 + i * 61, 108, "HOLD", TFT_GOLD);
    }
    if (logic_.screen() == Casino::Screen::VideoHold && logic_.videoCursor() == i)
      canvas.drawRoundRect(10 + i * 61, 53, 42, 52, 5, TFT_GREEN);
  }
  if (logic_.videoAnimating()) {
    CydUi::centered(canvas, logic_.videoDrawing() ? "Drawing..." : "Dealing...",
                    151, 2, TFT_CYAN);
  } else if (logic_.screen() == Casino::Screen::VideoHold) {
    CydUi::touchAction(canvas, VIDEO_PAYTABLE, "", TFT_CYAN, false);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY, CydUi::SURFACE_RAISED);
    canvas.drawString("View", VIDEO_PAYTABLE.x +
                                 (VIDEO_PAYTABLE.w - canvas.textWidth("View")) / 2,
                      VIDEO_PAYTABLE.y + 7);
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_WHITE, CydUi::SURFACE_RAISED);
    canvas.drawString("Payouts",
                      VIDEO_PAYTABLE.x +
                          (VIDEO_PAYTABLE.w - canvas.textWidth("Payouts")) / 2,
                      VIDEO_PAYTABLE.y + 20);
    CydUi::touchAction(canvas, VIDEO_DRAW, "Draw", TFT_GREEN);
    CydUi::centered(canvas, "Tap cards to hold", 130, 1, TFT_LIGHTGREY);
  } else {
    CydUi::centered(canvas, Casino::pokerScoreName(logic_.videoScore()), 130, 2, logic_.lastWin() > 0 ? TFT_GREEN : TFT_RED);
    CydUi::centered(canvas, "Paid " + money(logic_.lastWin()), 153, 1, TFT_GOLD);
    CydUi::touchAction(canvas, VIDEO_RESULT_ACTIONS[0], "Deal", TFT_GREEN);
    CydUi::touchAction(canvas, VIDEO_RESULT_ACTIONS[1], "Bets", TFT_CYAN, false);
    CydUi::touchAction(canvas, VIDEO_RESULT_ACTIONS[2], "Casino", TFT_LIGHTGREY, false);
  }
}

template <typename Canvas>
void MicriCasinoGame::drawVideoPaytable(Canvas& canvas) {
  canvas.fillRoundRect(16, 35, 288, 166, 8, TFT_NAVY);
  canvas.drawRoundRect(16, 35, 288, 166, 8, TFT_CYAN);
  CydUi::centered(canvas, "Jacks or Better Pays", 45, 2, TFT_WHITE);

  static const char* const hands[9] = {
      "Royal Flush", "Straight Flush", "Four Kind", "Full House", "Flush",
      "Straight", "Three Kind", "Two Pair", "Jacks+"};
  static const uint16_t multipliers[9] = {250, 50, 25, 9, 6, 4, 3, 2, 1};
  canvas.setTextSize(1);
  for (uint8_t i = 0; i < 9; ++i) {
    const uint8_t column = i < 5 ? 0 : 1;
    const uint8_t row = column == 0 ? i : static_cast<uint8_t>(i - 5);
    const int16_t x = column == 0 ? 28 : 170;
    const int16_t y = static_cast<int16_t>(70 + row * 18);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_NAVY);
    canvas.drawString(hands[i], x, y);
    canvas.setTextColor(TFT_GOLD, TFT_NAVY);
    canvas.drawRightString(String(multipliers[i]) + "x", x + 128, y, 1);
  }
  CydUi::touchAction(canvas, VIDEO_PAYTABLE_CLOSE, "Close", TFT_CYAN, false);
}

template <typename Canvas>
void MicriCasinoGame::drawBroke(Canvas& canvas) {
  drawShell(canvas, "Broke");
  CydUi::largeValue(canvas, "$0", 42, TFT_RED);
  CydUi::centered(canvas, "Restore your bankroll", 83, 2, TFT_WHITE);
  CydUi::touchAction(canvas, PRIMARY_ACTION, "Reset to $200", TFT_GREEN);
}

template <typename Canvas>
void MicriCasinoGame::drawCashOut(Canvas& canvas) {
  drawShell(canvas, "Cash Out");
  CydUi::centered(canvas, "Cashed Out", 42, 2, TFT_GOLD);
  CydUi::largeValue(canvas, money(logic_.lastCashout()), 65, TFT_GREEN);
  CydUi::centered(canvas, "New bankroll: $200", 116, 1, TFT_LIGHTGREY);
  CydUi::touchAction(canvas, PRIMARY_ACTION, "Return to Casino", TFT_GREEN);
}

void MicriCasinoGame::drawEnd(TFT_eSPI& tft) {
  ensureLoaded();
  CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    drawShell(canvas, "Micri Casino");
    CydUi::centered(canvas, "Saved", 48, 3, TFT_GREEN);
    CydUi::centered(canvas, "Bank " + money(logic_.bankroll()), 88, 2, TFT_GOLD);
    CydUi::touchAction(canvas, PRIMARY_ACTION, "Play Again", TFT_GREEN);
  });
}
