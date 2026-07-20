#include "RandomNumberApp.h"
#include "../../CydFramebuffer.h"
#include "../../CydUi.h"
#include <TFT_eSPI.h>

RandomNumberApp::RandomNumberApp(uint32_t width, uint32_t height)
    : App("Random Number", width, height) {}

namespace {
constexpr uint8_t RANGES_PER_PAGE = 6;
constexpr uint8_t RANGE_PAGE_COUNT =
    (RandomNumberLogic::RANGE_COUNT + RANGES_PER_PAGE - 1) / RANGES_PER_PAGE;
constexpr TouchUi::Rect ZERO_TOGGLE = {20, 60, 280, 46};
constexpr TouchUi::Rect ZERO_NEXT = {80, 130, 160, 48};
constexpr TouchUi::Rect RANGE_PREV = {12, 158, 50, 40};
constexpr TouchUi::Rect RANGE_NEXT = {68, 158, 50, 40};
constexpr TouchUi::Rect RANGE_GENERATE = {128, 158, 180, 40};
constexpr TouchUi::Rect RESULT_GENERATE = {18, 153, 184, 43};
constexpr TouchUi::Rect RESULT_EDIT = {212, 153, 90, 43};

TouchUi::Rect rangeRect(uint8_t slot) {
  return {static_cast<int16_t>(12 + (slot % 3) * 101),
          static_cast<int16_t>(46 + (slot / 3) * 51), 94, 43};
}
}

bool RandomNumberApp::handleTouch(const TouchUi::TouchSample& sample,
                                  const TouchUi::TouchEvent& event) {
  const TouchUi::Point point = TouchUi::currentPoint(sample, event);
  int16_t hit = TouchUi::NO_CONTROL;

  if (logic_.isChoosingIncludeZero()) {
    if (ZERO_TOGGLE.contains(point)) hit = 0;
    if (ZERO_NEXT.contains(point)) hit = 1;
  } else if (logic_.isChoosingRange()) {
    for (uint8_t slot = 0; slot < RANGES_PER_PAGE; ++slot) {
      const uint8_t index = rangePage_ * RANGES_PER_PAGE + slot;
      if (index < RandomNumberLogic::RANGE_COUNT && rangeRect(slot).contains(point)) {
        hit = slot;
        break;
      }
    }
    if (RANGE_PREV.contains(point)) hit = 6;
    if (RANGE_NEXT.contains(point)) hit = 7;
    if (RANGE_GENERATE.contains(point)) hit = 8;
  } else {
    if (RESULT_GENERATE.contains(point)) hit = 0;
    if (RESULT_EDIT.contains(point)) hit = 1;
  }

  const auto result = touchCapture_.update(sample, event, hit);
  if (result.pressed || result.released) markDirty();
  if (!result.activated) return result.consumed;

  if (logic_.isChoosingIncludeZero()) {
    if (result.id == 0) logic_.setIncludeZero(!logic_.includesZero());
    if (result.id == 1) logic_.beginRangeSelection();
  } else if (logic_.isChoosingRange()) {
    if (result.id >= 0 && result.id < RANGES_PER_PAGE) {
      const uint8_t index = rangePage_ * RANGES_PER_PAGE + result.id;
      if (index < RandomNumberLogic::RANGE_COUNT) logic_.chooseRange(index);
    } else if (result.id == 6) {
      rangePage_ = rangePage_ == 0 ? RANGE_PAGE_COUNT - 1 : rangePage_ - 1;
    } else if (result.id == 7) {
      rangePage_ = (rangePage_ + 1) % RANGE_PAGE_COUNT;
    } else if (result.id == 8) {
      logic_.roll();
    }
  } else {
    if (result.id == 0) logic_.roll();
    if (result.id == 1) logic_.beginEditing();
  }
  markDirty();
  return true;
}

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
  rangePage_ = 0;
  touchCapture_.reset();
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
  CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    if (logic_.isEditing()) {
      if (logic_.isChoosingIncludeZero()) {
        CydUi::header(canvas, "Random Number", TFT_MAGENTA, "ZERO");
        CydUi::touchToggle(canvas, ZERO_TOGGLE, "Include zero",
                           logic_.includesZero(),
                           touchCapture_.activeId() == 0);
        CydUi::touchAction(canvas, ZERO_NEXT, "Choose range", TFT_MAGENTA,
                           true, touchCapture_.activeId() == 1);
      } else {
        CydUi::header(canvas, "Random Number", TFT_MAGENTA, "RANGE");
        for (uint8_t slot = 0; slot < RANGES_PER_PAGE; ++slot) {
          const uint8_t index = rangePage_ * RANGES_PER_PAGE + slot;
          if (index >= RandomNumberLogic::RANGE_COUNT) break;
          String label = String(RandomNumberLogic::getRangeAt(index));
          CydUi::touchAction(canvas, rangeRect(slot), label.c_str(),
                             index == logic_.getRangeIndex() ? TFT_MAGENTA : TFT_CYAN,
                             index == logic_.getRangeIndex(),
                             touchCapture_.activeId() == slot);
        }
        CydUi::touchAction(canvas, RANGE_PREV, "<", TFT_BLUE, false,
                           touchCapture_.activeId() == 6);
        CydUi::touchAction(canvas, RANGE_NEXT, ">", TFT_BLUE, false,
                           touchCapture_.activeId() == 7);
        CydUi::touchAction(canvas, RANGE_GENERATE, "Generate", TFT_GREEN,
                           true, touchCapture_.activeId() == 8);
      }
    } else {
      String range = String(logic_.getMin()) + ".." + String(logic_.getMax());
      CydUi::header(canvas, "Random Number", TFT_GREEN, range.c_str());

      String res = String(logic_.getResult());
      CydUi::largeValue(canvas, res, 67, TFT_GREEN);
      CydUi::touchAction(canvas, RESULT_GENERATE, "Generate", TFT_GREEN,
                         true, touchCapture_.activeId() == 0);
      CydUi::touchAction(canvas, RESULT_EDIT, "Edit", TFT_LIGHTGREY, false,
                         touchCapture_.activeId() == 1);
    }
  });
  dirty_ = false;
}

void RandomNumberApp::drawStart(TFT_eSPI& tft) {
  if (!startDirty_) {
    return;
  }
  CydUi::clear(tft);
  CydUi::header(tft, "Random Number", TFT_MAGENTA);
  CydUi::centered(tft, "Pick a range", 65, 4, TFT_MAGENTA);
  CydUi::centered(tft, "Then roll", 126, 2, TFT_LIGHTGREY);
  CydUi::footer(tft, "Choose a range");
  startDirty_ = false;
}

bool RandomNumberApp::hasCustomOverlay() const { return true; }
bool RandomNumberApp::startsRunningImmediately() const { return true; }
