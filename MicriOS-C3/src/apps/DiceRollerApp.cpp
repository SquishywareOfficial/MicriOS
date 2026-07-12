#include "DiceRollerApp.h"

#include <Arduino.h>
#include <U8g2lib.h>

DiceRollerApp::DiceRollerApp(uint32_t width, uint32_t height, uint32_t left)
    : App("Dice Roller", width, height) {
  (void)left;
}

bool DiceRollerApp::startsRunningImmediately() const {
  return true;
}

bool DiceRollerApp::hasCustomOverlay() const {
  return true;
}

void DiceRollerApp::onAppReset() {
  logic_.reset();
}

void DiceRollerApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  logic_.update(deltaMs, input.click, input.longPress);
}

void DiceRollerApp::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_4x6_tr);

  if (logic_.getMode() == DiceRollerLogic::Mode::Select) {
    u8g2.drawStr(3, 8, "Select die");
    u8g2.setFont(u8g2_font_7x13_tr);

    char die[8];
    snprintf(die, sizeof(die), "d%u", logic_.getSelectedDice());
    const int x = (width + 2 - u8g2.getStrWidth(die)) / 2;
    u8g2.drawStr(x, 25, die);

    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(3, 39, "Tap next Hold roll");
    return;
  }

  char die[8];
  snprintf(die, sizeof(die), "d%u", logic_.getSelectedDice());
  u8g2.drawStr(3, 8, die);
  u8g2.drawRFrame(23, 9, 26, 20, 3);
  u8g2.setFont(u8g2_font_7x13B_tr);
  char value[8];
  snprintf(value, sizeof(value), "%u", logic_.isRolling() ? (uint8_t)random(1, logic_.getSelectedDice()+1) : logic_.getResult());
  u8g2.drawStr(36 - (u8g2.getStrWidth(value) / 2), 24, value);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 39, "Tap=roll Hold=die");
}
