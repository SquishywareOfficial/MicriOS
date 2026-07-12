#include "SimonGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
Preferences simonPrefs;
constexpr uint16_t SIMON_HOLD_MS = 360;
}

SimonGame::SimonGame(uint32_t width, uint32_t height, uint32_t left)
    : App("Simon", width, height), left_(left) {}

bool SimonGame::hasCustomOverlay() const {
  return true;
}

void SimonGame::onAppReset() {
  loadBest();
  length_ = 0;
  showIndex_ = 0;
  inputIndex_ = 0;
  timerMs_ = 0;
  mode_ = Mode::Show;
  flashLong_ = false;
  expectedLong_ = false;
  pressHandled_ = false;
  addStep();
}

void SimonGame::addStep() {
  if (length_ < 32) {
    sequence_[length_] = length_ == 0 ? false : random(0, 2) == 1;
    length_++;
  }
  showIndex_ = 0;
  inputIndex_ = 0;
  timerMs_ = 0;
  mode_ = Mode::Show;
  pressHandled_ = false;
}

void SimonGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  timerMs_ += deltaMs;
  if (mode_ == Mode::Show) {
    const uint16_t stepMs = 650;
    if (timerMs_ >= stepMs) {
      timerMs_ = 0;
      showIndex_++;
      if (showIndex_ >= length_) {
        mode_ = Mode::Input;
        inputIndex_ = 0;
        timerMs_ = 0;
        pressHandled_ = false;
      }
    }
    return;
  }

  if (mode_ == Mode::Good) {
    if (timerMs_ > 650) {
      addStep();
    }
    return;
  }

  if (timerMs_ < 260) {
    return;
  }

  if (input.down && !pressHandled_ && input.holdMs >= SIMON_HOLD_MS) {
    pressHandled_ = true;
    checkStep(true);
    return;
  }

  if (input.released) {
    if (!pressHandled_) {
      checkStep(false);
    }
    pressHandled_ = false;
  }
}

void SimonGame::checkStep(bool longTone) {
  flashLong_ = longTone;
  expectedLong_ = sequence_[inputIndex_];
  if (sequence_[inputIndex_] != longTone) {
    endApp();
    return;
  }
  inputIndex_++;
  if (inputIndex_ >= length_) {
    if (length_ > bestLevel_) {
      bestLevel_ = length_;
      saveBest();
    }
    mode_ = Mode::Good;
    timerMs_ = 0;
    pressHandled_ = false;
  }
}

void SimonGame::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 3, 7);
  u8g2.print("L");
  u8g2.print(length_);

  if (mode_ == Mode::Show) {
    const bool longTone = sequence_[showIndex_];
    if ((timerMs_ % 650) < (longTone ? 420 : 180)) {
      const char* cue = longTone ? "HOLD" : "TAP";
      u8g2.setFont(u8g2_font_7x13B_tr);
      u8g2.drawStr(left_ + ((width - u8g2.getStrWidth(cue)) / 2), 25, cue);
    }
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(left_ + 23, 38, "Watch");
    return;
  }

  if (mode_ == Mode::Good) {
    u8g2.setFont(u8g2_font_7x13_tr);
    u8g2.drawStr(left_ + 20, 25, "GOOD");
    return;
  }

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(left_ + 18, 19, "Repeat");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(left_ + 5, 30, "Tap=short");
  u8g2.drawStr(left_ + 38, 30, "Hold");
  u8g2.setCursor(left_ + 25, 38);
  u8g2.print(inputIndex_);
  u8g2.print("/");
  u8g2.print(length_);
}

void SimonGame::drawStart(U8G2& u8g2) {
  loadBest();
  u8g2.drawFrame(0, 0, width + 2, height);
  if (showStartPromptPage()) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(20, 16, "Press");
    u8g2.drawStr(13, 29, "to Start");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 10, "Best Memory");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(3, 24);
    if (bestLevel_ == 0) u8g2.print("--");
    else {
      u8g2.print(initials);
      u8g2.print(" L");
      u8g2.print(bestLevel_);
    }
  } else {
    u8g2.drawFrame(42, 12, 12, 12);
    u8g2.drawFrame(56, 12, 12, 12);
    u8g2.drawFrame(42, 26, 12, 12);
    u8g2.drawFrame(56, 26, 12, 12);
    u8g2.drawBox(56, 12, 12, 12);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 10, "Simon");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(3, 22, "Copy flashes");
    u8g2.drawStr(3, 31, "Tap=short");
    u8g2.drawStr(3, 38, "Hold=long");
  }
}

void SimonGame::drawEnd(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, "Wrong");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(3, 20);
  u8g2.print("Needed ");
  u8g2.print(expectedLong_ ? "HOLD" : "TAP");
  u8g2.setCursor(3, 29);
  u8g2.print("Best ");
  u8g2.print(bestLevel_);
  u8g2.drawStr(3, 38, "Tap retry Hold menu");
}

void SimonGame::loadBest() {
  if (bestLoaded_) return;
  simonPrefs.begin("simon", true);
  bestLevel_ = simonPrefs.getUChar("best", 0);
  bestInitials_ = simonPrefs.getUShort("init", PlayerProfile::defaultInitials());
  simonPrefs.end();
  bestLoaded_ = true;
}

void SimonGame::saveBest() {
  bestInitials_ = PlayerProfile::loadInitials();
  simonPrefs.begin("simon", false);
  simonPrefs.putUChar("best", bestLevel_);
  simonPrefs.putUShort("init", bestInitials_);
  simonPrefs.end();
}
