#include "FishingFlickGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
constexpr uint8_t FISH_LED_PIN = 8;
constexpr bool FISH_LED_ACTIVE_LOW = true;
Preferences fishPrefs;
}

FishingFlickGame::FishingFlickGame(uint32_t width, uint32_t height, uint32_t left)
    : App("Fishing Flick", width, height), left_(left) {}

bool FishingFlickGame::hasCustomOverlay() const {
  return true;
}

void FishingFlickGame::onAppReset() {
  pinMode(FISH_LED_PIN, OUTPUT);
  setWarningLed(false);
  loadBestFish();
  level_ = 1;
  caughtWeight_ = 0;
  loadLevel();
}

void FishingFlickGame::loadLevel() {
  fishState_ = FishState::Waiting;
  waitMs_ = static_cast<uint16_t>(random(700, 1900));
  elapsedMs_ = 0;
  hookTimerMs_ = 0;
  fightTimerMs_ = static_cast<uint16_t>(random(500, 1000));
  caughtTimerMs_ = 0;
  fishWeight_ = static_cast<uint16_t>(180 + (level_ * 120) + random(0, 140 + level_ * 25));
  progress_ = 0.0f;
  tension_ = 34.0f;
  fighting_ = false;
  setWarningLed(false);
}

void FishingFlickGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  const float deltaSec = static_cast<float>(deltaMs) * 0.001f;

  if (fishState_ == FishState::Waiting) {
    elapsedMs_ += deltaMs;
    if (elapsedMs_ >= waitMs_) {
      fishState_ = FishState::Hooking;
      hookTimerMs_ = 0;
    }
    setWarningLed(false);
    return;
  }

  if (fishState_ == FishState::Hooking) {
    hookTimerMs_ += deltaMs;
    if (input.pressed) {
      fishState_ = FishState::Reeling;
      tension_ = 36.0f;
      progress_ = 6.0f;
      fighting_ = false;
      fightTimerMs_ = 650;
      return;
    }
    if (hookTimerMs_ >= 950) {
      fishState_ = FishState::Escaped;
      setWarningLed(false);
      endApp();
    }
    return;
  }

  if (fishState_ != FishState::Reeling) {
    if (fishState_ == FishState::Caught) {
      caughtTimerMs_ += deltaMs;
      if (caughtTimerMs_ >= 1100) {
        level_++;
        loadLevel();
      }
    }
    setWarningLed(false);
    return;
  }

  if (fightTimerMs_ > deltaMs) {
    fightTimerMs_ -= deltaMs;
  } else {
    fighting_ = !fighting_;
    if (fighting_) {
      startFightBurst();
    }
    fightTimerMs_ = static_cast<uint16_t>(random(fighting_ ? 360 : 550, fighting_ ? 760 : 1200));
  }

  const float hard = difficulty();
  if (input.down) {
    progress_ += (fighting_ ? (4.0f / hard) : (32.0f / hard)) * deltaSec;
    tension_ += (fighting_ ? (42.0f + hard * 12.0f) : (7.0f + hard * 2.0f)) * deltaSec;
  } else {
    progress_ -= (fighting_ ? 2.0f : 0.8f) * deltaSec;
    tension_ -= (fighting_ ? (74.0f + hard * 7.0f) : (42.0f + hard * 4.0f)) * deltaSec;
  }

  tension_ = max(0.0f, min(110.0f, tension_));
  progress_ = max(0.0f, min(100.0f, progress_));
  updateWarningLed();

  if (tension_ <= 0.0f) {
    fishState_ = FishState::Escaped;
    setWarningLed(false);
    endApp();
    return;
  }

  if (tension_ >= 100.0f) {
    fishState_ = FishState::Broken;
    setWarningLed(false);
    endApp();
    return;
  }

  if (progress_ >= 100.0f) {
    fishState_ = FishState::Caught;
    caughtTimerMs_ = 0;
    caughtWeight_ = fishWeight_;
    if (caughtWeight_ > bestWeight_) {
      bestWeight_ = caughtWeight_;
      saveBestFish();
    }
    setWarningLed(false);
  }
}

void FishingFlickGame::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  drawWater(u8g2);
  u8g2.setFont(u8g2_font_4x6_tr);

  if (fishState_ == FishState::Waiting) {
    u8g2.drawStr(left_ + 18, 13, "Waiting");
    u8g2.setCursor(left_ + 3, 7);
    u8g2.print("L");
    u8g2.print(level_);
    u8g2.print(" ");
    u8g2.print(fishWeight_);
    u8g2.print("g");
    u8g2.drawLine(left_ + 34, 12, left_ + 34, 24);
    u8g2.drawPixel(left_ + 34, 25);
    return;
  }

  if (fishState_ == FishState::Hooking) {
    u8g2.drawStr(left_ + 18, 13, "TUG!");
    const int tugOffset = ((millis() / 120) % 2 == 0) ? 3 : -3;
    u8g2.drawLine(left_ + 34, 12, left_ + 34 + tugOffset, 24);
    u8g2.drawPixel(left_ + 34 + tugOffset, 25);
    u8g2.drawStr(left_ + 18, 36, "Tap!");
    return;
  }

  if (fishState_ == FishState::Caught) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(left_ + 14, 17, "Caught!");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(left_ + 18, 28);
    u8g2.print(caughtWeight_);
    u8g2.print("g");
    return;
  }

  u8g2.setCursor(left_ + 3, 7);
  u8g2.print("L");
  u8g2.print(level_);
  u8g2.print(" ");
  u8g2.print(fighting_ ? "FIGHT" : "REEL");
  u8g2.drawFrame(left_ + 5, 13, 58, 5);
  u8g2.drawBox(left_ + 6, 14, static_cast<int>(progress_ * 0.56f), 3);
  u8g2.drawStr(left_ + 5, 27, "T");
  u8g2.drawFrame(left_ + 14, 22, 49, 5);
  u8g2.drawBox(left_ + 15, 23, static_cast<int>(min(47.0f, tension_ * 0.47f)), 3);
  if (tension_ > 78.0f) {
    u8g2.drawStr(left_ + 22, 36, "LINE!");
  } else if (tension_ < 15.0f) {
    u8g2.drawStr(left_ + 22, 36, "SLACK!");
  }
}

void FishingFlickGame::drawStart(U8G2& u8g2) {
  loadBestFish();
  u8g2.drawFrame(0, 0, width + 2, height);
  if (showStartPromptPage()) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(20, 16, "Press");
    u8g2.drawStr(13, 29, "to Start");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 10, "Top Fish");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(3, 24);
    if (bestWeight_ == 0) {
      u8g2.print("--");
    } else {
      u8g2.print(initials);
      u8g2.print(" ");
      u8g2.print(bestWeight_);
      u8g2.print("g");
    }
  } else {
    drawWater(u8g2);
    u8g2.drawLine(12, 9, 24, 25);
    u8g2.drawLine(24, 25, 31, 24);
    u8g2.drawPixel(33, 25);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 10, appTitle());
  }
}

void FishingFlickGame::drawEnd(U8G2& u8g2) {
  setWarningLed(false);
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  const char* title = fishState_ == FishState::Escaped ? "Escaped" : "Line broke";
  u8g2.drawStr(3, 9, title);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(3, 20);
  u8g2.print("Fish ");
  if (caughtWeight_ == 0) {
    u8g2.print("--");
  } else {
    u8g2.print(caughtWeight_);
    u8g2.print("g");
  }
  u8g2.setCursor(3, 29);
  u8g2.print("Best ");
  if (bestWeight_ == 0) {
    u8g2.print("--");
  } else {
    u8g2.print(bestWeight_);
    u8g2.print("g");
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.print(" ");
    u8g2.print(initials);
  }
  u8g2.drawStr(3, 38, "Tap retry Hold menu");
}

float FishingFlickGame::difficulty() const {
  return min(3.3f, 0.75f + static_cast<float>(level_ - 1) * 0.22f);
}

void FishingFlickGame::startFightBurst() {
  tension_ += 7.0f + (difficulty() * 3.5f);
  if (tension_ > 110.0f) {
    tension_ = 110.0f;
  }
}

void FishingFlickGame::loadBestFish() {
  if (bestLoaded_) {
    return;
  }
  fishPrefs.begin("fish", true);
  bestWeight_ = fishPrefs.getUShort("best", 0);
  bestInitials_ = fishPrefs.getUShort("init", PlayerProfile::defaultInitials());
  fishPrefs.end();
  bestLoaded_ = true;
}

void FishingFlickGame::saveBestFish() {
  bestInitials_ = PlayerProfile::loadInitials();
  fishPrefs.begin("fish", false);
  fishPrefs.putUShort("best", bestWeight_);
  fishPrefs.putUShort("init", bestInitials_);
  fishPrefs.end();
}

void FishingFlickGame::setWarningLed(bool on) {
  digitalWrite(FISH_LED_PIN, FISH_LED_ACTIVE_LOW ? !on : on);
}

void FishingFlickGame::updateWarningLed() {
  if (tension_ < 80.0f) {
    setWarningLed(false);
    return;
  }
  setWarningLed(((millis() / 120) % 2) == 0);
}

void FishingFlickGame::drawWater(U8G2& u8g2) {
  u8g2.drawLine(left_ + 1, 29, left_ + width - 2, 29);
  for (uint8_t i = 0; i < 5; i++) {
    const int x = left_ + 4 + i * 14;
    u8g2.drawLine(x, 33, x + 5, 31);
    u8g2.drawLine(x + 5, 31, x + 10, 33);
  }
}
