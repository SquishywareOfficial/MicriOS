#include "DiceRollerApp.h"
#include "../../CydFramebuffer.h"
#include "../../CydUi.h"
#include <TFT_eSPI.h>

DiceRollerApp::DiceRollerApp(uint32_t width, uint32_t height)
    : App("Dice Roller", width, height) {}

namespace {
TouchUi::Rect dieRect(uint8_t index) {
  const uint8_t row = index / 4;
  const uint8_t col = index % 4;
  return {static_cast<int16_t>(12 + col * 77),
          static_cast<int16_t>(48 + row * 66), 68, 54};
}
constexpr TouchUi::Rect ROLL_AGAIN = {18, 153, 184, 43};
constexpr TouchUi::Rect CHANGE_DIE = {212, 153, 90, 43};
}

bool DiceRollerApp::handleTouch(const TouchUi::TouchSample& sample,
                                const TouchUi::TouchEvent& event) {
  const TouchUi::Point point = sample.down ? sample.point : event.point;
  if (logic_.getMode() == DiceRollerLogic::Mode::Select) {
    for (uint8_t i = 0; i < DiceRollerLogic::DICE_COUNT; ++i) {
      if (!dieRect(i).contains(point)) continue;
      if (event.tap) {
        logic_.selectDice(DiceRollerLogic::DICE[i]);
        logic_.startRolling();
        dirty_ = true;
      }
      return true;
    }
    return false;
  }

  if (ROLL_AGAIN.contains(point)) {
    if (event.tap && !logic_.isRolling()) {
      logic_.startRolling();
      dirty_ = true;
    }
    return true;
  }
  if (CHANGE_DIE.contains(point)) {
    if (event.tap && !logic_.isRolling()) {
      logic_.reset();
      dirty_ = true;
    }
    return true;
  }
  return false;
}

void DiceRollerApp::onAppReset() {
  logic_.reset();
  dirty_ = true;
  renderedRollFrame_ = UINT16_MAX;
}

void DiceRollerApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  const auto oldMode = logic_.getMode();
  const uint8_t oldDie = logic_.getSelectedDice();
  const uint8_t oldResult = logic_.getResult();
  const bool oldRolling = logic_.isRolling();
  logic_.update(deltaMs, b1.click || b2.click, b1.longPress);
  if (oldMode != logic_.getMode() || oldDie != logic_.getSelectedDice() ||
      oldResult != logic_.getResult() || oldRolling != logic_.isRolling()) {
    dirty_ = true;
  }
}

void DiceRollerApp::drawRunning(TFT_eSPI& tft) {
  if (logic_.getMode() == DiceRollerLogic::Mode::Select) {
    if (!dirty_ && renderedMode_ == logic_.getMode() && renderedDie_ == logic_.getSelectedDice()) {
      return;
    }
    CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
      CydUi::header(canvas, "Choose a die", TFT_CYAN);
      for (uint8_t i = 0; i < DiceRollerLogic::DICE_COUNT; ++i) {
        String die = "d" + String(DiceRollerLogic::DICE[i]);
        CydUi::touchAction(canvas, dieRect(i), die.c_str(), TFT_CYAN, false);
      }
    });
  } else {
    String die = "d" + String(logic_.getSelectedDice());

    if (logic_.isRolling()) {
      const uint16_t frame = logic_.getAnimMs() / 90;
      if (!dirty_ && renderedRollFrame_ == frame) {
        return;
      }
      CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
        CydUi::header(canvas, "Dice Roller", TFT_YELLOW, die.c_str());
        CydUi::centered(canvas, "Rolling...", 129, 2, TFT_LIGHTGREY);
        String val = String(random(1, logic_.getSelectedDice() + 1));
        CydUi::largeValue(canvas, val, 72, TFT_YELLOW);
      });
      renderedRollFrame_ = frame;
    } else {
      if (!dirty_ && renderedMode_ == logic_.getMode() && renderedResult_ == logic_.getResult()) {
        return;
      }
      CydFramebuffer::draw(tft, width, height, [&](auto& canvas) {
        CydUi::header(canvas, "Dice Roller", TFT_GREEN, die.c_str());
        String val = String(logic_.getResult());
        CydUi::largeValue(canvas, val, 68, TFT_GREEN);
        CydUi::touchAction(canvas, ROLL_AGAIN, "Roll again", TFT_GREEN);
        CydUi::touchAction(canvas, CHANGE_DIE, "Change", TFT_LIGHTGREY,
                           false);
      });
    }
  }
  renderedMode_ = logic_.getMode();
  renderedDie_ = logic_.getSelectedDice();
  renderedResult_ = logic_.getResult();
  dirty_ = false;
}

void DiceRollerApp::drawStart(TFT_eSPI& tft) {
  CydUi::clear(tft);
  CydUi::header(tft, "Dice Roller", TFT_CYAN);
  CydUi::centered(tft, "d4  d6  d8", 64, 3, TFT_WHITE);
  CydUi::centered(tft, "d10  d12  d20  d100", 112, 2, TFT_CYAN);
  CydUi::footer(tft, "Choose a die");
}

bool DiceRollerApp::hasCustomOverlay() const { return true; }

bool DiceRollerApp::startsRunningImmediately() const { return true; }
