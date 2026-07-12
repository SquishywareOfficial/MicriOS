#include "MetronomeApp.h"
#include <U8g2lib.h>
#include <Arduino.h>

MetronomeApp::MetronomeApp(uint32_t width, uint32_t height, uint32_t left)
    : App("Metronome", width, height) {
  (void)left;
}

void MetronomeApp::onAppReset() {
  logic_.reset();
}

void MetronomeApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  (void)deltaMs;
  if (input.click) {
      if (logic_.isRunning()) logic_.toggle();
      else {
          uint16_t b = logic_.getBpm() + 5;
          if (b > 300) b = 20;
          logic_.setBpm(b);
      }
  }
  if (input.longPress) {
      logic_.toggle();
  }

  if (logic_.update(millis())) {
      lastTickMs_ = millis();
  }
}

void MetronomeApp::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 8, "Metronome");

  u8g2.setFont(u8g2_font_7x13_tr);
  char buf[16];
  snprintf(buf, sizeof(buf), "%u BPM", logic_.getBpm());
  int x = (int)((width + 2 - u8g2.getStrWidth(buf)) / 2);
  u8g2.drawStr(x, 24, buf);

  if (logic_.isRunning() && (millis() - lastTickMs_) < 100) {
      u8g2.drawBox(x, 26, u8g2.getStrWidth(buf), 2);
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  if (logic_.isRunning()) {
      u8g2.drawStr(3, 39, "Tap/Hold to stop");
  } else {
      u8g2.drawStr(3, 33, "Tap +5 BPM");
      u8g2.drawStr(3, 39, "Hold to start");
  }
}

bool MetronomeApp::startsRunningImmediately() const {
  return true;
}

bool MetronomeApp::hasCustomOverlay() const {
  return true;
}
