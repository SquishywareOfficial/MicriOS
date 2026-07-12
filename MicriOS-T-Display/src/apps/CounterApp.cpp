#include "CounterApp.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"
#include <TFT_eSPI.h>

CounterApp::CounterApp(uint32_t width, uint32_t height)
    : App("Counter", width, height) {}

void CounterApp::render(TFT_eSPI& tft) {
  const AppPhase currentPhase = phase();
  if (!phaseCached_ || currentPhase != renderedPhase_) {
    phaseCached_ = true;
    renderedPhase_ = currentPhase;
    dirty_ = true;
    startDirty_ = true;
  }
  App::render(tft);
}

void CounterApp::markDirty() {
  dirty_ = true;
}

void CounterApp::onAppReset() {
  logic_.reset();
  markDirty();
}

void CounterApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)deltaMs;
  if (b1.click) {
    logic_.increment();
    markDirty();
  }
  if (b2.click && logic_.getCount() > 0) {
    logic_.setCount(logic_.getCount() - 1);
    markDirty();
  }
  if (b1.longPress) {
    logic_.reset();
    markDirty();
  }
}

void CounterApp::drawRunning(TFT_eSPI& tft) {
  if (!dirty_) {
    return;
  }
  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    TDisplayUi::header(canvas, "Counter", TFT_GREEN);
    TDisplayUi::pill(canvas, 174, 36, "B2 -1", TFT_CYAN);

    String val = String(logic_.getCount());
    TDisplayUi::largeValue(canvas, val, 55, TFT_GREEN);

    TDisplayUi::footer(canvas, "B1 +1 / B1 hold reset / B2 -1");
  });
  dirty_ = false;
}

void CounterApp::drawStart(TFT_eSPI& tft) {
  if (!startDirty_) {
    return;
  }
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Counter", TFT_GREEN);
  TDisplayUi::centered(tft, "Tap to count", 50, 2, TFT_WHITE);
  TDisplayUi::centered(tft, "B2 counts down", 78, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(tft, "B1 start");
  startDirty_ = false;
}

bool CounterApp::hasCustomOverlay() const { return true; }
