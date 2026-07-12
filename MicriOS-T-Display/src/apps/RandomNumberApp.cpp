#include "RandomNumberApp.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"
#include <TFT_eSPI.h>

RandomNumberApp::RandomNumberApp(uint32_t width, uint32_t height)
    : App("Random Number", width, height) {}

void RandomNumberApp::render(TFT_eSPI& tft) {
  const AppPhase currentPhase = phase();
  if (!phaseCached_ || currentPhase != renderedPhase_) {
    phaseCached_ = true;
    renderedPhase_ = currentPhase;
    dirty_ = true;
    startDirty_ = true;
  }
  App::render(tft);
}

void RandomNumberApp::markDirty() {
  dirty_ = true;
}

void RandomNumberApp::onAppReset() {
  logic_.reset();
  markDirty();
}

void RandomNumberApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  const RandomNumberLogic::Mode oldMode = logic_.getMode();
  const bool oldIncludeZero = logic_.includesZero();
  const uint8_t oldRangeIndex = logic_.getRangeIndex();
  const uint32_t oldResult = logic_.getResult();
  logic_.update(deltaMs, b1.click, b1.longPress);
  if (oldMode != logic_.getMode() || oldIncludeZero != logic_.includesZero() ||
      oldRangeIndex != logic_.getRangeIndex() || oldResult != logic_.getResult()) {
    markDirty();
  }
}

void RandomNumberApp::drawRunning(TFT_eSPI& tft) {
  if (!dirty_) {
    return;
  }
  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    if (logic_.isEditing()) {
      if (logic_.isChoosingIncludeZero()) {
        TDisplayUi::header(canvas, "Random Number", TFT_MAGENTA, "ZERO");
        TDisplayUi::centered(canvas, "Include 0?", 42, 2, TFT_LIGHTGREY);
        TDisplayUi::centered(canvas, logic_.includesZero() ? "YES" : "NO", 70, 4, logic_.includesZero() ? TFT_GREEN : TFT_RED);
        TDisplayUi::footer(canvas, "B1 toggle / B1 hold next");
      } else {
        TDisplayUi::header(canvas, "Random Number", TFT_MAGENTA, "RANGE");
        String range = String(logic_.getMin()) + ".." + String(logic_.getMax());
        TDisplayUi::centered(canvas, "Upper bound", 38, 1, TFT_LIGHTGREY);
        TDisplayUi::largeValue(canvas, String(logic_.getMax()), 55, TFT_MAGENTA);
        TDisplayUi::centered(canvas, range, 104, 1, TFT_LIGHTGREY);
        TDisplayUi::footer(canvas, "B1 next range / B1 hold roll");
      }
    } else {
      String range = String(logic_.getMin()) + ".." + String(logic_.getMax());
      TDisplayUi::header(canvas, "Random Number", TFT_GREEN, range.c_str());

      String res = String(logic_.getResult());
      TDisplayUi::largeValue(canvas, res, 49, TFT_GREEN);

      TDisplayUi::footer(canvas, "B1 roll again / B1 hold edit");
    }
  });
  dirty_ = false;
}

void RandomNumberApp::drawStart(TFT_eSPI& tft) {
  if (!startDirty_) {
    return;
  }
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Random Number", TFT_MAGENTA);
  TDisplayUi::centered(tft, "Pick a range", 50, 3, TFT_MAGENTA);
  TDisplayUi::centered(tft, "Then roll", 86, 1, TFT_LIGHTGREY);
  TDisplayUi::footer(tft, "B1 start");
  startDirty_ = false;
}

bool RandomNumberApp::hasCustomOverlay() const { return true; }
bool RandomNumberApp::startsRunningImmediately() const { return true; }
