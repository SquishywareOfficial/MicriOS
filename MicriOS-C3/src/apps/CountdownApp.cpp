#include "CountdownApp.h"
#include <U8g2lib.h>

CountdownApp::CountdownApp(uint32_t width, uint32_t height, uint32_t left)
    : App("Countdown", width, height) {
  (void)left;
}

void CountdownApp::onAppReset() {
  logic_.reset();
}

void CountdownApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  (void)deltaMs;
  if (input.click) {
    if (logic_.isRunning()) logic_.toggle(millis());
    else logic_.adjustDuration(10000); // +10s
  }
  if (input.longPress) {
    if (logic_.isRunning()) logic_.reset();
    else logic_.toggle(millis());
  }
}

void CountdownApp::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 8, "Countdown");

  uint32_t remaining = logic_.getRemainingMs(millis());
  uint32_t s = (remaining / 1000) % 60;
  uint32_t m = (remaining / 60000) % 60;
  uint32_t h = (remaining / 3600000);

  char buf[32];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
  u8g2.setFont(u8g2_font_7x13_tr);
  int x = (int)((width + 2 - u8g2.getStrWidth(buf)) / 2);

  if (remaining == 0 && (millis() / 250) % 2 == 0) {
      // flash when finished
  } else {
      u8g2.drawStr(x, 24, buf);
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  if (logic_.isRunning()) {
      u8g2.drawStr(3, 33, "Tap stop");
      u8g2.drawStr(3, 39, "Hold reset");
  } else {
      u8g2.drawStr(3, 33, "Tap +10s");
      u8g2.drawStr(3, 39, "Hold start");
  }
}

void CountdownApp::drawStart(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 10, "Countdown");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 24, "Hold to start");
}

bool CountdownApp::hasCustomOverlay() const {
  return true;
}
