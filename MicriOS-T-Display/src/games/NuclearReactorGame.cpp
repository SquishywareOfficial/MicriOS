#include "NuclearReactorGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
Preferences reactorPrefs;
}

NuclearReactorGame::NuclearReactorGame(uint32_t width, uint32_t height)
    : App("Reactor", width, height) {}

bool NuclearReactorGame::hasCustomOverlay() const {
  return true;
}

void NuclearReactorGame::onAppReset() {
  loadBest();
  temp_ = 58.0f;
  rodsMs_ = 0;
  instability_ = 0.0f;
  currentPowerKw_ = 0.0f;
  scoreWh_ = 0;
  secondMs_ = 0;
  survivedSec_ = 0;
  meltdown_ = false;
  stalled_ = false;
}

void NuclearReactorGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)b2;
  if (b1.pressed) {
    rodsMs_ = 520;
  }

  const float dt = static_cast<float>(deltaMs) * 0.001f;
  if (rodsMs_ > 0) {
    rodsMs_ = deltaMs >= rodsMs_ ? 0 : rodsMs_ - deltaMs;
  }

  instability_ += dt * 0.018f;
  const float noise = static_cast<float>(random(-18, 19)) * 0.020f * (1.0f + instability_);
  const float heat = 9.0f + instability_ * 2.8f + noise + max(0.0f, (temp_ - 70.0f) * 0.035f);
  const float cooling = rodsMs_ > 0 ? 58.0f : 3.0f;
  temp_ += (heat - cooling) * dt;
  if (temp_ < 0.0f) temp_ = 0.0f;
  currentPowerKw_ = max(0.0f, (temp_ - 28.0f) * 24.0f);

  secondMs_ += deltaMs;
  while (secondMs_ >= 1000) {
    secondMs_ -= 1000;
    survivedSec_++;
    scoreWh_ += static_cast<uint32_t>(currentPowerKw_ / 3.6f);
    if (scoreWh_ > bestWh_) {
      bestWh_ = scoreWh_;
      saveBest();
    }
  }

  if (temp_ >= 100.0f) {
    meltdown_ = true;
    endApp();
  } else if (temp_ <= 20.0f) {
    stalled_ = true;
    endApp();
  }
}

void NuclearReactorGame::drawRunning(TFT_eSPI& tft) {
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    const uint16_t tempColor = temp_ >= 86.0f ? TFT_RED : (temp_ <= 30.0f ? TFT_CYAN : TFT_YELLOW);
    TDisplayUi::clear(canvas);
    TDisplayUi::header(canvas, "Reactor", tempColor, rodsMs_ > 0 ? "RODS" : "HOT");
    TDisplayUi::labelValue(canvas, 38, "Power", String(static_cast<unsigned long>(currentPowerKw_)) + " kW", TFT_GREEN);
    TDisplayUi::labelValue(canvas, 62, "Total", String(static_cast<unsigned long>(scoreWh_ / 1000)) + " kWh", TFT_CYAN);
    TDisplayUi::labelValue(canvas, 86, "Temp", String(static_cast<int>(temp_)) + " C", tempColor);
    TDisplayUi::bar(canvas, 132, 90, 88, 13, temp_ / 100.0f, tempColor);
    TDisplayUi::footer(canvas, "B1 inserts cooling rods");
  });
}

void NuclearReactorGame::drawStart(TFT_eSPI& tft) { tft.fillScreen(TFT_BLACK);
  loadBest();
  TDisplayUi::clear(tft);
  if (showStartPromptPage()) {
    TDisplayUi::header(tft, "Reactor", TFT_YELLOW);
    TDisplayUi::centered(tft, "Press to Start", 56, 2, TFT_WHITE);
    TDisplayUi::footer(tft, "B1 cools rods");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    TDisplayUi::header(tft, "Top Power", TFT_YELLOW);
    String best = bestWh_ == 0 ? "--" : String(initials) + " " + String(static_cast<unsigned long>(bestWh_ / 1000)) + "kWh";
    TDisplayUi::largeValue(tft, best, 54, TFT_GREEN);
    TDisplayUi::footer(tft, "Best generated energy");
  } else {
    TDisplayUi::header(tft, "Reactor", TFT_YELLOW);
    const int cx = 148;
    const int cy = 68;
    tft.fillCircle(cx, cy, 7, TFT_BLACK);
    tft.drawCircle(cx, cy, 7, TFT_YELLOW);
    for (uint8_t i = 0; i < 3; i++) {
      const float a = i * 2.094f;
      const int x1 = cx + static_cast<int>(cosf(a) * 10);
      const int y1 = cy + static_cast<int>(sinf(a) * 10);
      const int x2 = cx + static_cast<int>(cosf(a - 0.48f) * 40);
      const int y2 = cy + static_cast<int>(sinf(a - 0.48f) * 40);
      const int x3 = cx + static_cast<int>(cosf(a + 0.48f) * 40);
      const int y3 = cy + static_cast<int>(sinf(a + 0.48f) * 40);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_YELLOW);
    }
    TDisplayUi::centered(tft, "Keep it hot", 104, 1, TFT_LIGHTGREY);
  }
}

void NuclearReactorGame::drawEnd(TFT_eSPI& tft) { tft.fillScreen(TFT_BLACK);
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, meltdown_ ? "Meltdown" : stalled_ ? "Stalled" : "Shutdown", meltdown_ ? TFT_RED : TFT_CYAN);
  TDisplayUi::labelValue(tft, 49, "Power", String(static_cast<unsigned long>(scoreWh_ / 1000)) + " kWh", TFT_GREEN);
  TDisplayUi::labelValue(tft, 78, "Time", String(survivedSec_) + "s", TFT_YELLOW);
  TDisplayUi::footer(tft, "B1 retry / B1 hold menu");
}

void NuclearReactorGame::loadBest() {
  if (bestLoaded_) return;
  reactorPrefs.begin("reactor", true);
  bestWh_ = reactorPrefs.getULong("best", 0);
  bestInitials_ = reactorPrefs.getUShort("init", PlayerProfile::defaultInitials());
  reactorPrefs.end();
  bestLoaded_ = true;
}

void NuclearReactorGame::saveBest() {
  bestInitials_ = PlayerProfile::loadInitials();
  reactorPrefs.begin("reactor", false);
  reactorPrefs.putULong("best", bestWh_);
  reactorPrefs.putUShort("init", bestInitials_);
  reactorPrefs.end();
}
