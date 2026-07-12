#include "SimonGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
Preferences simonPrefs;
constexpr uint16_t SIMON_HOLD_MS = 360;
}

SimonGame::SimonGame(uint32_t width, uint32_t height)
    : App("Simon", width, height) {}

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

void SimonGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)b2;
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

  if (b1.down && !pressHandled_ && b1.holdMs >= SIMON_HOLD_MS) {
    pressHandled_ = true;
    checkStep(true);
    return;
  }

  if (b1.released) {
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

void SimonGame::drawRunning(TFT_eSPI& tft) {
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    TDisplayUi::clear(canvas);
    TDisplayUi::header(canvas, "Simon", TFT_MAGENTA, (String("L") + String(length_)).c_str());

    if (mode_ == Mode::Show) {
      const bool longTone = sequence_[showIndex_];
      const uint16_t color = longTone ? TFT_ORANGE : TFT_CYAN;
      if ((timerMs_ % 650) < (longTone ? 420 : 180)) {
        const char* cue = longTone ? "HOLD" : "TAP";
        canvas.fillRoundRect(42, 42, 156, 48, 8, color);
        canvas.setTextSize(4);
        canvas.setTextColor(TFT_BLACK, color);
        canvas.drawString(cue, (width - canvas.textWidth(cue)) / 2, 52);
      } else {
        canvas.drawRoundRect(42, 42, 156, 48, 8, TFT_DARKGREY);
      }
      TDisplayUi::footer(canvas, "Watch the sequence");
      return;
    }

    if (mode_ == Mode::Good) {
      TDisplayUi::centered(canvas, "GOOD", 52, 4, TFT_GREEN);
      TDisplayUi::footer(canvas, "Next round...");
      return;
    }

    TDisplayUi::centered(canvas, "REPEAT", 37, 3, TFT_WHITE);
    TDisplayUi::row(canvas, 75, "B1 tap = short", false, TFT_CYAN);
    TDisplayUi::footer(canvas, (String("Step ") + String(inputIndex_ + 1) + "/" + String(length_) + " / hold = long").c_str());
  });
}

void SimonGame::drawStart(TFT_eSPI& tft) { tft.fillScreen(TFT_BLACK);
  loadBest();
  TDisplayUi::clear(tft);
  if (showStartPromptPage()) {
    TDisplayUi::header(tft, "Simon", TFT_MAGENTA);
    TDisplayUi::centered(tft, "Press to Start", 56, 2, TFT_WHITE);
    TDisplayUi::footer(tft, "Copy TAP and HOLD cues");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    TDisplayUi::header(tft, "Best Memory", TFT_MAGENTA);
    String score = bestLevel_ == 0 ? "--" : String(initials) + " L" + String(bestLevel_);
    TDisplayUi::largeValue(tft, score, 54, TFT_MAGENTA);
    TDisplayUi::footer(tft, "Longest sequence");
  } else {
    TDisplayUi::header(tft, "Simon", TFT_MAGENTA);
    tft.fillRoundRect(52, 45, 58, 34, 6, TFT_CYAN);
    tft.fillRoundRect(130, 45, 58, 34, 6, TFT_ORANGE);
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK, TFT_CYAN);
    tft.drawString("TAP", 63, 54);
    tft.setTextColor(TFT_BLACK, TFT_ORANGE);
    tft.drawString("HOLD", 136, 54);
    TDisplayUi::centered(tft, "Copy the flashes", 94, 1, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "Short tap or long hold");
  }
}

void SimonGame::drawEnd(TFT_eSPI& tft) { tft.fillScreen(TFT_BLACK);
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Wrong", TFT_RED);
  TDisplayUi::labelValue(tft, 49, "Needed", expectedLong_ ? "HOLD" : "TAP", expectedLong_ ? TFT_ORANGE : TFT_CYAN);
  TDisplayUi::labelValue(tft, 78, "Best", String(bestLevel_), TFT_GREEN);
  TDisplayUi::footer(tft, "B1 retry / B1 hold menu");
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
