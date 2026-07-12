#include "NuclearReactorGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
Preferences reactorPrefs;
}

NuclearReactorGame::NuclearReactorGame(uint32_t width, uint32_t height, uint32_t left)
    : App("Reactor", width, height), left_(left) {}

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

void NuclearReactorGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  if (input.pressed) {
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

void NuclearReactorGame::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 3, 7);
  u8g2.print(static_cast<unsigned long>(scoreWh_ / 1000));
  u8g2.print("kWh");
  u8g2.drawStr(left_ + 49, 7, rodsMs_ > 0 ? "ROD" : "HOT");

  u8g2.drawFrame(left_ + 4, 12, 10, 25);
  const uint8_t fill = constrain(static_cast<int>(temp_ * 0.23f), 0, 23);
  u8g2.drawBox(left_ + 5, 36 - fill, 8, fill);
  u8g2.drawFrame(left_ + 18, 11, 50, 7);
  u8g2.drawBox(left_ + 19, 12, constrain(static_cast<int>(temp_ * 0.48f), 0, 48), 5);

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(left_ + 18, 29);
  u8g2.print(static_cast<unsigned long>(currentPowerKw_));
  u8g2.print("kW");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 18, 38);
  u8g2.print(static_cast<int>(temp_));
  u8g2.print("C Tap rods");
}

void NuclearReactorGame::drawStart(U8G2& u8g2) {
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
    u8g2.drawStr(3, 10, "Top Power");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(3, 24);
    if (bestWh_ == 0) u8g2.print("--");
    else {
      u8g2.print(initials);
      u8g2.print(" ");
      u8g2.print(static_cast<unsigned long>(bestWh_ / 1000));
      u8g2.print("kWh");
    }
  } else {
    const int cx = 53;
    const int cy = 23;
    u8g2.drawCircle(cx, cy, 3);
    for (uint8_t i = 0; i < 3; i++) {
      const float a = i * 2.094f;
      const int x1 = cx + static_cast<int>(cosf(a) * 5);
      const int y1 = cy + static_cast<int>(sinf(a) * 5);
      const int x2 = cx + static_cast<int>(cosf(a - 0.55f) * 13);
      const int y2 = cy + static_cast<int>(sinf(a - 0.55f) * 13);
      const int x3 = cx + static_cast<int>(cosf(a + 0.55f) * 13);
      const int y3 = cy + static_cast<int>(sinf(a + 0.55f) * 13);
      u8g2.drawTriangle(x1, y1, x2, y2, x3, y3);
    }
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 10, "Reactor");
  }
}

void NuclearReactorGame::drawEnd(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, meltdown_ ? "Meltdown" : stalled_ ? "Stalled" : "Shutdown");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(3, 20);
  u8g2.print("Power ");
  u8g2.print(static_cast<unsigned long>(scoreWh_ / 1000));
  u8g2.print("kWh");
  u8g2.setCursor(3, 29);
  u8g2.print("Time ");
  u8g2.print(survivedSec_);
  u8g2.print("s");
  u8g2.drawStr(3, 38, "Tap retry Hold menu");
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
