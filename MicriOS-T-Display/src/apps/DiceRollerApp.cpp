#include "DiceRollerApp.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"
#include <TFT_eSPI.h>

DiceRollerApp::DiceRollerApp(uint32_t width, uint32_t height)
    : App("Dice Roller", width, height) {}

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
    TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
      TDisplayUi::header(canvas, "Dice Roller", TFT_CYAN, "SELECT");
      String die = "d" + String(logic_.getSelectedDice());
      TDisplayUi::largeValue(canvas, die, 52, TFT_CYAN);
      TDisplayUi::footer(canvas, "B1 next die / B1 hold roll");
    });
  } else {
    String die = "d" + String(logic_.getSelectedDice());

    if (logic_.isRolling()) {
      const uint16_t frame = logic_.getAnimMs() / 90;
      if (!dirty_ && renderedRollFrame_ == frame) {
        return;
      }
      TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
        TDisplayUi::header(canvas, "Dice Roller", TFT_YELLOW, die.c_str());
        TDisplayUi::footer(canvas, "Rolling...");
        String val = String(random(1, logic_.getSelectedDice() + 1));
        TDisplayUi::largeValue(canvas, val, 52, TFT_YELLOW);
      });
      renderedRollFrame_ = frame;
    } else {
      if (!dirty_ && renderedMode_ == logic_.getMode() && renderedResult_ == logic_.getResult()) {
        return;
      }
      TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
        TDisplayUi::header(canvas, "Dice Roller", TFT_GREEN, die.c_str());
        String val = String(logic_.getResult());
        TDisplayUi::largeValue(canvas, val, 48, TFT_GREEN);
        TDisplayUi::footer(canvas, "B1 roll again / B1 hold dice select");
      });
    }
  }
  renderedMode_ = logic_.getMode();
  renderedDie_ = logic_.getSelectedDice();
  renderedResult_ = logic_.getResult();
  dirty_ = false;
}

void DiceRollerApp::drawStart(TFT_eSPI& tft) {
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Dice Roller", TFT_CYAN);
  TDisplayUi::centered(tft, "d4 d6 d8", 44, 2, TFT_WHITE);
  TDisplayUi::centered(tft, "d10 d12 d20 d100", 72, 2, TFT_CYAN);
  TDisplayUi::footer(tft, "B1 start");
}

bool DiceRollerApp::hasCustomOverlay() const { return true; }

bool DiceRollerApp::startsRunningImmediately() const { return true; }
