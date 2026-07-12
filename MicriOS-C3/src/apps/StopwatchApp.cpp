#include "StopwatchApp.h"
#include <U8g2lib.h>

StopwatchApp::StopwatchApp(uint32_t width, uint32_t height, uint32_t left)
    : App("Stopwatch", width, height) {
  (void)left;
}

void StopwatchApp::onAppReset() {
  logic_.reset();
}

void StopwatchApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  (void)deltaMs;
  if (input.click) {
    logic_.toggle(millis());
  }
  if (input.longPress) {
    logic_.reset();
  }
}

void StopwatchApp::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 8, "Stopwatch");

  uint32_t elapsed = logic_.getElapsedMs(millis());
  uint32_t ms = elapsed % 1000;
  uint32_t s = (elapsed / 1000) % 60;
  uint32_t m = (elapsed / 60000) % 60;
  uint32_t h = (elapsed / 3600000);

  char buf[32];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
  u8g2.setFont(u8g2_font_7x13_tr);
  int x = (int)((width + 2 - u8g2.getStrWidth(buf)) / 2);
  u8g2.drawStr(x, 24, buf);

  u8g2.setFont(u8g2_font_4x6_tr);
  snprintf(buf, sizeof(buf), ".%03u", (unsigned)ms);
  u8g2.drawStr(x + 56, 24, buf);

  u8g2.drawStr(3, 33, logic_.isRunning() ? "Tap stop" : "Tap start");
  const char* holdText = "Hold reset";
  int holdW = u8g2.getStrWidth(holdText);
  u8g2.drawStr(width + 2 - holdW - 3, 39, holdText);
}

void StopwatchApp::drawStart(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 10, "Stopwatch");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 24, "Tap to start");
  u8g2.drawStr(3, 36, "Hold=Reset");
}

bool StopwatchApp::hasCustomOverlay() const {
  return true;
}
