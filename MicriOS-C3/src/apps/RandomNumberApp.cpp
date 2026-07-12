#include "RandomNumberApp.h"
#include <Arduino.h>
#include <U8g2lib.h>

RandomNumberApp::RandomNumberApp(uint32_t width, uint32_t height, uint32_t left)
    : App("Random Number", width, height) {
  (void)left;
}

bool RandomNumberApp::startsRunningImmediately() const {
  return true;
}

bool RandomNumberApp::hasCustomOverlay() const {
  return true;
}

void RandomNumberApp::onAppReset() {
  logic_.reset();
}

void RandomNumberApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  logic_.update(deltaMs, input.click, input.longPress);
}

void RandomNumberApp::drawNumber(U8G2& u8g2, uint32_t value, int y) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(value));
  u8g2.setFont(u8g2_font_7x13_tr);
  if (u8g2.getStrWidth(buf) > static_cast<int>(width - 4)) {
    u8g2.setFont(u8g2_font_5x8_tr);
  }
  const int x = (width + 2 - u8g2.getStrWidth(buf)) / 2;
  u8g2.drawStr(x, y, buf);
}

void RandomNumberApp::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 8, "Random");

  if (logic_.isEditing()) {
    u8g2.setFont(u8g2_font_5x8_tr);
    if (logic_.isChoosingIncludeZero()) {
        u8g2.drawStr(3, 20, "Include 0?");
        u8g2.setFont(u8g2_font_7x13_tr);
        u8g2.drawStr(logic_.includesZero() ? 24 : 28, 33, logic_.includesZero() ? "YES" : "NO");
    } else {
        u8g2.drawStr(3, 18, "Max:");
        drawNumber(u8g2, logic_.getMax(), 32);
    }
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(3, 39, logic_.isChoosingIncludeZero() ? "Tap swap Hold next" : "Tap range Hold roll");
    return;
  }

  drawNumber(u8g2, logic_.getResult(), 24);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(3, 8);
  u8g2.print(logic_.getMin());
  u8g2.print("..");
  u8g2.print(logic_.getMax());
  u8g2.drawStr(3, 37, "Tap roll Hold edit");
}
