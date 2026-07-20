#include "CoinFlipperApp.h"
#include "../../CydFramebuffer.h"
#include "../../CydUi.h"
#include <TFT_eSPI.h>

namespace {
constexpr int COIN_CX = 160;
constexpr int COIN_CY = 101;
constexpr int COIN_R = 31;
constexpr TouchUi::Rect COIN_TOUCH = {119, 61, 82, 82};
constexpr TouchUi::Rect FLIP_ACTION = {86, 160, 148, 38};

template <typename Canvas>
void drawCoinBase(Canvas& tft, uint16_t rim, uint16_t fill) {
  tft.drawCircle(COIN_CX, COIN_CY, COIN_R, rim);
  tft.drawCircle(COIN_CX, COIN_CY, COIN_R - 2, rim);
  tft.fillCircle(COIN_CX, COIN_CY, COIN_R - 5, fill);
}

template <typename Canvas>
void drawHeadCoin(Canvas& tft) {
  drawCoinBase(tft, TFT_GOLD, TFT_DARKGREY);
  tft.fillCircle(COIN_CX, COIN_CY - 7, 8, TFT_GOLD);
  tft.drawFastHLine(COIN_CX - 16, COIN_CY + 14, 32, TFT_GOLD);
  tft.drawFastHLine(COIN_CX - 10, COIN_CY + 10, 20, TFT_GOLD);
  tft.drawLine(COIN_CX - 9, COIN_CY - 19, COIN_CX - 5, COIN_CY - 26, TFT_GOLD);
  tft.drawLine(COIN_CX, COIN_CY - 18, COIN_CX, COIN_CY - 28, TFT_GOLD);
  tft.drawLine(COIN_CX + 9, COIN_CY - 19, COIN_CX + 5, COIN_CY - 26, TFT_GOLD);
  tft.drawFastHLine(COIN_CX - 10, COIN_CY - 18, 20, TFT_GOLD);
}

template <typename Canvas>
void drawTailCoin(Canvas& tft) {
  drawCoinBase(tft, TFT_SILVER, TFT_DARKGREY);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(3);
  tft.setTextColor(TFT_SILVER, TFT_DARKGREY);
  tft.drawString("$1", COIN_CX - 18, COIN_CY - 14);
}
}

CoinFlipperApp::CoinFlipperApp(uint32_t width, uint32_t height)
    : App("Coin Flipper", width, height) {}

bool CoinFlipperApp::handleTouch(const TouchUi::TouchSample& sample,
                                 const TouchUi::TouchEvent& event) {
  if (logic_.getMode() == CoinFlipperLogic::Mode::Flipping) return false;
  const TouchUi::Point point = TouchUi::currentPoint(sample, event);
  int16_t hit = TouchUi::NO_CONTROL;
  if (COIN_TOUCH.contains(point)) hit = 0;
  if (FLIP_ACTION.contains(point)) hit = 1;
  const auto result = touchCapture_.update(sample, event, hit);
  if (result.pressed || result.released) dirty_ = true;
  if (result.activated) {
    logic_.startFlip();
    renderedFrame_ = UINT16_MAX;
    dirty_ = true;
  }
  return result.consumed;
}

void CoinFlipperApp::onAppReset() {
  logic_.reset();
  dirty_ = true;
  renderedFrame_ = UINT16_MAX;
  touchCapture_.reset();
}

void CoinFlipperApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  const auto oldMode = logic_.getMode();
  const bool oldHeads = logic_.isHeads();
  logic_.update(deltaMs, b1.click, b1.longPress);
  if (oldMode != logic_.getMode() || oldHeads != logic_.isHeads()) {
    dirty_ = true;
  }
}

void CoinFlipperApp::drawRunning(TFT_eSPI& tft) {
  if (logic_.getMode() == CoinFlipperLogic::Mode::Ready) {
    if (!dirty_ && renderedMode_ == logic_.getMode()) {
      return;
    }
    CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
      CydUi::header(canvas, "Coin Flipper", TFT_GOLD, "READY");
      drawTailCoin(canvas);
      CydUi::touchAction(canvas, FLIP_ACTION, "Flip", TFT_GOLD, true,
                         touchCapture_.activeId() == 1);
    });
  } else if (logic_.getMode() == CoinFlipperLogic::Mode::Flipping) {
    const uint16_t frame = (logic_.getAnimMs() / 110) % 4;
    if (!dirty_ && renderedFrame_ == frame) {
      return;
    }
    CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
      CydUi::header(canvas, "Coin Flipper", TFT_YELLOW, "FLIP");
      CydUi::centered(canvas, "Flipping...", 170, 2, TFT_LIGHTGREY);
      const int coinW = frame == 0 ? 58 : (frame == 1 || frame == 3 ? 18 : 36);
      canvas.drawEllipse(COIN_CX, COIN_CY, coinW / 2, 30, TFT_YELLOW);
      canvas.fillEllipse(COIN_CX, COIN_CY, max(2, coinW / 2 - 4), 25, frame == 2 ? TFT_DARKGREY : TFT_GOLD);
    });
    renderedFrame_ = frame;
  } else {
    if (!dirty_ && renderedMode_ == logic_.getMode() && renderedHeads_ == logic_.isHeads()) {
      return;
    }
    CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
      CydUi::header(canvas, "Coin Flipper", logic_.isHeads() ? TFT_GOLD : TFT_SILVER, "RESULT");
      canvas.setTextDatum(TL_DATUM);
      if (logic_.isHeads()) {
        drawHeadCoin(canvas);
        canvas.setTextSize(2);
        canvas.setTextColor(TFT_GOLD, TFT_BLACK);
        canvas.drawString("HEADS", (width - canvas.textWidth("HEADS")) / 2, 143);
      } else {
        drawTailCoin(canvas);
        canvas.setTextSize(2);
        canvas.setTextColor(TFT_SILVER, TFT_BLACK);
        canvas.drawString("TAILS", (width - canvas.textWidth("TAILS")) / 2, 143);
      }
      CydUi::touchAction(canvas, FLIP_ACTION, "Flip again", TFT_GOLD, true,
                         touchCapture_.activeId() == 1);
    });
  }
  renderedMode_ = logic_.getMode();
  renderedHeads_ = logic_.isHeads();
  dirty_ = false;
}

void CoinFlipperApp::drawStart(TFT_eSPI& tft) {
  CydUi::clear(tft);
  CydUi::header(tft, "Coin Flipper", TFT_GOLD);
  drawHeadCoin(tft);
  CydUi::centered(tft, "Heads or tails", 143, 2, TFT_LIGHTGREY);
  CydUi::footer(tft, "Tap Flip");
}

bool CoinFlipperApp::hasCustomOverlay() const { return true; }
bool CoinFlipperApp::startsRunningImmediately() const { return true; }
