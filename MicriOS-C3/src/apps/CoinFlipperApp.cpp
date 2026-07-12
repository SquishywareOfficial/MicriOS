#include "CoinFlipperApp.h"

#include <Arduino.h>
#include <U8g2lib.h>

CoinFlipperApp::CoinFlipperApp(uint32_t width, uint32_t height, uint32_t left)
    : App("Coin Flipper", width, height) {
  (void)left;
}

bool CoinFlipperApp::startsRunningImmediately() const {
  return true;
}

bool CoinFlipperApp::hasCustomOverlay() const {
  return true;
}

void CoinFlipperApp::onAppReset() {
  logic_.reset();
}

void CoinFlipperApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  logic_.update(deltaMs, input.click, input.longPress);
}

void CoinFlipperApp::drawCoin(U8G2& u8g2, int cx, int cy, uint8_t frame) {
  if (frame == 0) {
    u8g2.drawCircle(cx, cy, 10);
    u8g2.drawCircle(cx, cy, 8);
  } else if (frame == 1) {
    u8g2.drawFrame(cx - 2, cy - 10, 4, 20);
  } else {
    u8g2.drawEllipse(cx, cy, 7, 10);
  }
}

void CoinFlipperApp::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 8, "Coin");

  if (logic_.getMode() == CoinFlipperLogic::Mode::Flipping) {
    drawCoin(u8g2, 52, 21, (logic_.getAnimMs() / 110) % 3);
    u8g2.drawStr(3, 22, "Flip");
    return;
  }

  u8g2.drawCircle(52, 21, 12);
  if (logic_.getMode() == CoinFlipperLogic::Mode::Ready) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(10, 24, "Tap");
  } else if (logic_.isHeads()) {
    u8g2.drawCircle(52, 18, 3);
    u8g2.drawLine(47, 24, 57, 24);
    u8g2.drawLine(49, 13, 51, 10);
    u8g2.drawLine(52, 13, 52, 9);
    u8g2.drawLine(55, 13, 53, 10);
    u8g2.setFont(u8g2_font_7x13_tr);
    u8g2.drawStr(3, 25, "HEAD");
    return;
  } else {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(45, 23, "$1");
    u8g2.setFont(u8g2_font_7x13_tr);
    u8g2.drawStr(3, 25, "TAIL");
    return;
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 32, "Tap flip");
  u8g2.drawStr(3, 38, "Hold menu");
}
