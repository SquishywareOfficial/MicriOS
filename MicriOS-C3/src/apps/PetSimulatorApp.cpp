#include "PetSimulatorApp.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

namespace {
Preferences petPrefs;
const char* PET_NAMES[] = {"Cat", "Dog", "Dino", "Blob"};
const char* ACTIONS[] = {"Feed", "Play", "Sleep", "Clean", "Stats", "Exit"};
constexpr uint8_t PET_COUNT = sizeof(PET_NAMES) / sizeof(PET_NAMES[0]);
constexpr uint8_t ACTION_COUNT = sizeof(ACTIONS) / sizeof(ACTIONS[0]);
constexpr uint32_t CARE_INTERVAL_MS = 30000;
constexpr uint32_t IDLE_AFTER_MS = 10000;
constexpr uint32_t IDLE_STEP_MS = 850;
constexpr uint32_t PLAY_DURATION_MS = 2600;
constexpr uint32_t PLAY_STEP_MS = 110;
constexpr uint32_t MESSAGE_DURATION_MS = 1200;
constexpr uint8_t LOW_HEALTH_WARNING = 20;

uint8_t addStat(uint8_t value, uint8_t amount) {
  return min<uint8_t>(100, value + amount);
}

uint8_t dropStat(uint8_t value, uint8_t amount) {
  return value > amount ? value - amount : 0;
}

int8_t stepToward(int8_t current, int8_t target) {
  if (current < target) return 1;
  if (current > target) return -1;
  return 0;
}
}

PetSimulatorApp::PetSimulatorApp(uint32_t width, uint32_t height, uint32_t left)
    : App("Pet Simulator", width, height) {
  (void)left;
}

bool PetSimulatorApp::startsRunningImmediately() const {
  return true;
}

bool PetSimulatorApp::hasCustomOverlay() const {
  return true;
}

void PetSimulatorApp::onAppReset() {
  loadPet();
  mode_ = loaded_ ? (health_ == 0 ? Mode::Dead : Mode::Menu) : Mode::ChoosePet;
  menuIndex_ = 0;
  careMs_ = 0;
  messageMs_ = 0;
  idleMs_ = 0;
  wanderMs_ = 0;
  playMs_ = 0;
  playStepMs_ = 0;
  petX_ = 34;
  petY_ = 21;
  petDx_ = 1;
  petDy_ = 0;
  toyX_ = 48;
  toyY_ = 19;
  idleMode_ = false;
}

void PetSimulatorApp::loadPet() {
  petPrefs.begin("pet", true);
  loaded_ = petPrefs.getBool("made", false);
  petType_ = petPrefs.getUChar("type", 0);
  hunger_ = petPrefs.getUChar("hung", 80);
  fun_ = petPrefs.getUChar("fun", 80);
  energy_ = petPrefs.getUChar("energy", 80);
  clean_ = petPrefs.getUChar("clean", 80);
  health_ = petPrefs.getUChar("health", 100);
  poop_ = petPrefs.getUChar("poop", 0);
  petPrefs.end();
  if (petType_ >= PET_COUNT) petType_ = 0;
}

void PetSimulatorApp::savePet() {
  petPrefs.begin("pet", false);
  petPrefs.putBool("made", true);
  petPrefs.putUChar("type", petType_);
  petPrefs.putUChar("hung", hunger_);
  petPrefs.putUChar("fun", fun_);
  petPrefs.putUChar("energy", energy_);
  petPrefs.putUChar("clean", clean_);
  petPrefs.putUChar("health", health_);
  petPrefs.putUChar("poop", poop_);
  petPrefs.end();
  loaded_ = true;
}

void PetSimulatorApp::resetCareStats() {
  hunger_ = 80;
  fun_ = 80;
  energy_ = 80;
  clean_ = 80;
  health_ = 100;
  poop_ = 0;
}

void PetSimulatorApp::tickCare(uint32_t deltaMs) {
  if (health_ == 0) {
    mode_ = Mode::Dead;
    return;
  }

  careMs_ += deltaMs;
  if (careMs_ < CARE_INTERVAL_MS) {
    return;
  }
  careMs_ = 0;
  hunger_ = dropStat(hunger_, 1);
  fun_ = dropStat(fun_, 1);
  energy_ = dropStat(energy_, 1);
  if (poop_ > 0) {
    clean_ = dropStat(clean_, poop_ >= 3 ? 2 : 1);
  }
  if (random(0, 10) == 0 && poop_ < 5) poop_++;

  const uint16_t average = (hunger_ + fun_ + energy_ + clean_) / 4;
  if (average < 20 || hunger_ == 0 || clean_ == 0) {
    health_ = dropStat(health_, 2);
  } else if (average < 35 || poop_ >= 4) {
    health_ = dropStat(health_, 1);
  } else if (average > 75 && poop_ == 0 && health_ < 100) {
    health_ = addStat(health_, 1);
  }
  if (health_ == 0) mode_ = Mode::Dead;
  savePet();
}

void PetSimulatorApp::applyAction() {
  if (health_ == 0) {
    mode_ = Mode::Dead;
    return;
  }

  switch (menuIndex_) {
    case 0:
      hunger_ = addStat(hunger_, 22);
      energy_ = addStat(energy_, 3);
      if (random(0, 3) == 0) poop_ = min<uint8_t>(5, poop_ + 1);
      message_ = "Yum";
      break;
    case 1:
      fun_ = addStat(fun_, 26);
      energy_ = dropStat(energy_, 10);
      hunger_ = dropStat(hunger_, 6);
      if (random(0, 3) == 0) clean_ = dropStat(clean_, 12);
      if (random(0, 8) == 0 && poop_ < 5) poop_++;
      savePet();
      startPlayScene();
      return;
    case 2:
      energy_ = addStat(energy_, 32);
      hunger_ = dropStat(hunger_, 5);
      fun_ = dropStat(fun_, 2);
      message_ = "Zzz";
      break;
    case 3:
      clean_ = addStat(clean_, 36);
      fun_ = dropStat(fun_, 6);
      if (health_ > 0 && health_ < 100) health_ = addStat(health_, 3);
      poop_ = 0;
      message_ = "Clean";
      break;
    case 4:
      mode_ = Mode::Stats;
      return;
    case 5:
      savePet();
      requestExitToMenu();
      return;
  }
  savePet();
  mode_ = Mode::Message;
  messageMs_ = 0;
}

void PetSimulatorApp::startPlayScene() {
  mode_ = Mode::PlayScene;
  idleMode_ = false;
  playMs_ = 0;
  playStepMs_ = 0;
  petX_ = 34;
  petY_ = 21;
  toyX_ = static_cast<int8_t>(random(11, static_cast<int>(width) - 10));
  toyY_ = static_cast<int8_t>(random(12, static_cast<int>(height) - 8));
}

void PetSimulatorApp::updatePlayScene(uint32_t deltaMs) {
  playMs_ += deltaMs;
  playStepMs_ += deltaMs;
  if (playStepMs_ >= PLAY_STEP_MS) {
    playStepMs_ = 0;
    if (abs(petX_ - toyX_) <= 1 && abs(petY_ - toyY_) <= 1) {
      toyX_ = static_cast<int8_t>(random(11, static_cast<int>(width) - 10));
      toyY_ = static_cast<int8_t>(random(12, static_cast<int>(height) - 8));
    }
    petX_ = constrain(static_cast<int>(petX_) + stepToward(petX_, toyX_), 9, static_cast<int>(width) - 9);
    petY_ = constrain(static_cast<int>(petY_) + stepToward(petY_, toyY_), 11, static_cast<int>(height) - 8);
  }
  if (playMs_ >= PLAY_DURATION_MS) {
    message_ = "Happy";
    messageMs_ = 0;
    mode_ = Mode::Message;
  }
}

void PetSimulatorApp::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  tickCare(deltaMs);
  updateIdle(deltaMs);

  if (idleMode_) {
    if (input.click || input.longPress || input.pressed) {
      idleMode_ = false;
      idleMs_ = 0;
    }
    return;
  }

  if (input.click || input.longPress || input.pressed) {
    idleMs_ = 0;
  }

  if (mode_ == Mode::ChoosePet) {
    if (input.click) {
      petType_ = (petType_ + 1) % PET_COUNT;
    }
    if (input.longPress) {
      resetCareStats();
      savePet();
      mode_ = Mode::Menu;
    }
    return;
  }

  if (mode_ == Mode::Dead) {
    if (input.longPress) {
      resetCareStats();
      loaded_ = false;
      mode_ = Mode::ChoosePet;
    }
    return;
  }

  if (mode_ == Mode::Stats) {
    if (input.click || input.longPress) {
      mode_ = Mode::Menu;
    }
    return;
  }

  if (mode_ == Mode::Message) {
    messageMs_ += deltaMs;
    if (messageMs_ > MESSAGE_DURATION_MS || input.click || input.longPress) {
      mode_ = Mode::Menu;
    }
    return;
  }

  if (mode_ == Mode::PlayScene) {
    updatePlayScene(deltaMs);
    return;
  }

  if (input.click) {
    menuIndex_ = (menuIndex_ + 1) % ACTION_COUNT;
  }
  if (input.longPress) {
    applyAction();
  }
}

void PetSimulatorApp::updateIdle(uint32_t deltaMs) {
  if (mode_ != Mode::Menu) {
    idleMs_ = 0;
    idleMode_ = false;
    return;
  }
  if (!idleMode_) {
    idleMs_ += deltaMs;
    if (idleMs_ >= IDLE_AFTER_MS) {
      idleMode_ = true;
      wanderMs_ = 0;
    }
    return;
  }

  wanderMs_ += deltaMs;
  if (wanderMs_ < IDLE_STEP_MS) {
    return;
  }
  wanderMs_ = 0;
  if (random(0, 4) == 0) {
    petDx_ = static_cast<int8_t>(random(-1, 2));
    petDy_ = static_cast<int8_t>(random(-1, 2));
    if (petDx_ == 0 && petDy_ == 0) petDx_ = 1;
  }
  petX_ = constrain(static_cast<int>(petX_) + petDx_, 9, static_cast<int>(width) - 9);
  petY_ = constrain(static_cast<int>(petY_) + petDy_, 11, static_cast<int>(height) - 8);
}

void PetSimulatorApp::drawPet(U8G2& u8g2, int x, int y) {
  if (petType_ == 0) {
    u8g2.drawCircle(x, y, 5);
    u8g2.drawLine(x - 4, y - 4, x - 6, y - 8);
    u8g2.drawLine(x + 4, y - 4, x + 6, y - 8);
  } else if (petType_ == 1) {
    u8g2.drawCircle(x, y, 5);
    u8g2.drawBox(x - 7, y - 4, 3, 5);
    u8g2.drawBox(x + 4, y - 4, 3, 5);
  } else if (petType_ == 2) {
    u8g2.drawTriangle(x - 6, y + 4, x + 7, y, x - 6, y - 5);
    u8g2.drawPixel(x + 2, y - 1);
  } else {
    u8g2.drawCircle(x, y, 6);
    u8g2.drawPixel(x - 2, y - 1);
    u8g2.drawPixel(x + 2, y - 1);
  }
}

void PetSimulatorApp::drawPoop(U8G2& u8g2, int x, int y) {
  u8g2.drawPixel(x, y);
  u8g2.drawPixel(x - 1, y + 1);
  u8g2.drawPixel(x, y + 1);
  u8g2.drawPixel(x + 1, y + 1);
}

void PetSimulatorApp::drawPoops(U8G2& u8g2) {
  for (uint8_t i = 0; i < poop_; i++) {
    const int x = 8 + (i * 13) % 55;
    const int y = height - 7 - (i % 2) * 3;
    drawPoop(u8g2, x, y);
  }
}

void PetSimulatorApp::drawLowHealthWarning(U8G2& u8g2) {
  if (health_ == 0 || health_ > LOW_HEALTH_WARNING || ((millis() / 350) % 2) == 0) {
    return;
  }
  u8g2.drawFrame(1, 1, width, height - 2);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(width - 18, 8, "HP!");
}

void PetSimulatorApp::drawBar(U8G2& u8g2, int x, int y, const char* label, uint8_t value) {
  u8g2.drawStr(x, y, label);
  u8g2.drawFrame(x + 16, y - 5, 28, 5);
  u8g2.drawBox(x + 17, y - 4, map(value, 0, 100, 0, 26), 3);
}

void PetSimulatorApp::drawRunning(U8G2& u8g2) {
  if (idleMode_) {
    u8g2.setFont(u8g2_font_4x6_tr);
    drawPoops(u8g2);
    drawPet(u8g2, petX_, petY_);
    drawLowHealthWarning(u8g2);
    return;
  }

  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_4x6_tr);

  if (mode_ == Mode::ChoosePet) {
    u8g2.drawStr(3, 8, "Choose pet");
    drawPet(u8g2, 36, 21);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 36, PET_NAMES[petType_]);
    return;
  }

  if (mode_ == Mode::Dead) {
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(18, 13, "RIP");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(7, 27, PET_NAMES[petType_]);
    u8g2.drawStr(3, 38, "Hold new pet");
    return;
  }

  if (mode_ == Mode::PlayScene) {
    drawPoops(u8g2);
    u8g2.drawDisc(toyX_, toyY_, 1);
    drawPet(u8g2, petX_, petY_);
    return;
  }

  drawPoops(u8g2);
  drawPet(u8g2, 17, 20);
  if (mode_ == Mode::Stats) {
    drawBar(u8g2, 31, 8, "H", hunger_);
    drawBar(u8g2, 31, 15, "F", fun_);
    drawBar(u8g2, 31, 22, "E", energy_);
    drawBar(u8g2, 31, 29, "C", clean_);
    u8g2.setCursor(31, 38);
    u8g2.print("HP");
    u8g2.print(health_);
    drawLowHealthWarning(u8g2);
    return;
  }

  if (mode_ == Mode::Message) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(34, 23, message_);
    drawLowHealthWarning(u8g2);
    return;
  }

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(31, 15, PET_NAMES[petType_]);
  u8g2.drawStr(31, 28, ACTIONS[menuIndex_]);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(3, 38, "Tap next Hold");
  drawLowHealthWarning(u8g2);
}
