#include "CounterApp.h"
#include "../../CydFramebuffer.h"
#include "../../CydUi.h"
#include <TFT_eSPI.h>

CounterApp::CounterApp(uint32_t width, uint32_t height)
    : App("Counter", width, height) {}

namespace {
constexpr TouchUi::Rect COUNTER_CONTROLS[] = {
    {18, 145, 86, 45}, {117, 145, 86, 45}, {216, 145, 86, 45}};
enum CounterControl : int16_t { Minus = 0, Reset = 1, Plus = 2 };
}

bool CounterApp::handleTouch(const TouchUi::TouchSample& sample,
                             const TouchUi::TouchEvent& event) {
  const int16_t hit = TouchUi::hitRectList(
      COUNTER_CONTROLS, 3, TouchUi::currentPoint(sample, event));
  const auto result = touchCapture_.update(sample, event, hit);
  if (result.pressed || result.released) markDirty();
  if (!result.activated) return result.consumed;

  if (result.id == Minus && logic_.getCount() > 0) {
    logic_.setCount(logic_.getCount() - 1);
  } else if (result.id == Plus) {
    logic_.increment();
  } else if (result.id == Reset) {
    logic_.reset();
  }
  markDirty();
  return true;
}

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
  touchCapture_.reset();
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
  CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    CydUi::header(canvas, "Counter", TFT_GREEN);
    String val = String(logic_.getCount());
    CydUi::largeValue(canvas, val, 67, TFT_GREEN);
    CydUi::touchAction(canvas, COUNTER_CONTROLS[Minus], "-1", TFT_CYAN,
                       true, touchCapture_.activeId() == Minus);
    CydUi::touchAction(canvas, COUNTER_CONTROLS[Reset], "Reset",
                       TFT_LIGHTGREY, false,
                       touchCapture_.activeId() == Reset);
    CydUi::touchAction(canvas, COUNTER_CONTROLS[Plus], "+1", TFT_GREEN,
                       true, touchCapture_.activeId() == Plus);
  });
  dirty_ = false;
}

void CounterApp::drawStart(TFT_eSPI& tft) {
  if (!startDirty_) {
    return;
  }
  CydUi::clear(tft);
  CydUi::header(tft, "Counter", TFT_GREEN);
  CydUi::centered(tft, "Tap to count", 72, 3, TFT_WHITE);
  CydUi::centered(tft, "Left decreases, right increases", 124, 2, TFT_LIGHTGREY);
  CydUi::footer(tft, "Tap +1 or -1");
  startDirty_ = false;
}

bool CounterApp::hasCustomOverlay() const { return true; }
bool CounterApp::startsRunningImmediately() const { return true; }
