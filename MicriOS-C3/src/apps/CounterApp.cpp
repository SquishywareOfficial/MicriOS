#include "CounterApp.h"

#include <U8g2lib.h>

CounterApp::CounterApp(uint32_t width, uint32_t height, uint32_t left)
    : App("Counter", width, height) {}

void CounterApp::onAppReset() {
  logic_.reset();
}

void CounterApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  (void)deltaMs;
  if (input.click) {
    // increment on tap
    logic_.increment();
  }
  if (input.longPress) {
    // reset on hold (do not exit to menu)
    logic_.reset();
  }
}

void CounterApp::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);

  char buf[16];
  snprintf(buf, sizeof(buf), "%u", (unsigned)logic_.getCount());

  u8g2.setFont(u8g2_font_7x13_tr);
  int textW = u8g2.getStrWidth(buf);
  int x = (int)((width + 2 - textW) / 2);
  u8g2.drawStr(x, 24, buf);

  // Hints: tap hint at top, hold hint at bottom-right
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 9, "Tap=+1");
  const char* holdText = "Hold=Reset";
  int holdW = u8g2.getStrWidth(holdText);
  int holdX = (int)(width + 2 - holdW - 3);
  if (holdX < 3) holdX = 3;
  u8g2.drawStr(holdX, 36, holdText);
}

void CounterApp::drawStart(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 10, "Counter");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 24, "Tap to increment");
  u8g2.drawStr(3, 36, "Hold=Reset");
}

bool CounterApp::hasCustomOverlay() const {
  return true;
}
