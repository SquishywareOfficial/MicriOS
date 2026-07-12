#include "PetSimulatorApp.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../TDisplayFramebuffer.h"

namespace {
Preferences petPrefs;
const char* PET_NAMES[] = {"Cat", "Dog", "Dino", "Blob"};
const char* ACTIONS[] = {"Feed", "Play", "Sleep", "Clean", "Stats", "Exit"};
constexpr uint8_t PET_COUNT = sizeof(PET_NAMES) / sizeof(PET_NAMES[0]);
constexpr uint8_t ACTION_COUNT = sizeof(ACTIONS) / sizeof(ACTIONS[0]);
constexpr uint32_t CARE_INTERVAL_MS = 30000;
constexpr uint32_t IDLE_AFTER_MS = 10000;
constexpr uint32_t IDLE_STEP_MS = 900;
constexpr uint32_t PLAY_DURATION_MS = 2800;
constexpr uint32_t PLAY_STEP_MS = 90;
constexpr uint32_t MESSAGE_DURATION_MS = 1200;
constexpr uint8_t LOW_HEALTH_WARNING = 20;

uint8_t addStat(uint8_t value, uint8_t amount) {
  return min<uint8_t>(100, value + amount);
}

uint8_t dropStat(uint8_t value, uint8_t amount) {
  return value > amount ? value - amount : 0;
}

int8_t stepToward(int16_t current, int16_t target, int8_t step) {
  if (current < target) return step;
  if (current > target) return -step;
  return 0;
}

template <typename Canvas>
void drawShell(Canvas& tft, uint32_t width, uint32_t height, const char* title) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString(title, 12, 8);
  tft.drawFastHLine(12, 34, width - 24, TFT_DARKGREY);
  tft.drawRect(0, 0, width, height, TFT_DARKGREY);
}

template <typename Canvas>
void drawFooter(Canvas& tft, uint32_t width, uint32_t height, const char* text) {
  tft.fillRect(0, height - 17, width, 17, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(text, 12, height - 13);
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
  petX_ = static_cast<int16_t>(width / 2);
  petY_ = static_cast<int16_t>(height / 2 + 8);
  petDx_ = 1;
  petDy_ = 0;
  toyX_ = static_cast<int16_t>(width * 2 / 3);
  toyY_ = static_cast<int16_t>(height / 2);
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
  petX_ = static_cast<int16_t>(width / 2);
  petY_ = static_cast<int16_t>(height / 2 + 8);
  toyX_ = static_cast<int16_t>(random(36, static_cast<int>(width) - 36));
  toyY_ = static_cast<int16_t>(random(48, static_cast<int>(height) - 28));
}

void PetSimulatorApp::updatePlayScene(uint32_t deltaMs) {
  playMs_ += deltaMs;
  playStepMs_ += deltaMs;
  if (playStepMs_ >= PLAY_STEP_MS) {
    playStepMs_ = 0;
    if (abs(petX_ - toyX_) <= 4 && abs(petY_ - toyY_) <= 4) {
      toyX_ = static_cast<int16_t>(random(36, static_cast<int>(width) - 36));
      toyY_ = static_cast<int16_t>(random(48, static_cast<int>(height) - 28));
    }
    petX_ = constrain(static_cast<int>(petX_) + stepToward(petX_, toyX_, 4), 22, static_cast<int>(width) - 22);
    petY_ = constrain(static_cast<int>(petY_) + stepToward(petY_, toyY_, 3), 48, static_cast<int>(height) - 28);
  }
  if (playMs_ >= PLAY_DURATION_MS) {
    message_ = "Happy";
    messageMs_ = 0;
    mode_ = Mode::Message;
  }
}

void PetSimulatorApp::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  tickCare(deltaMs);
  updateIdle(deltaMs);

  if (idleMode_) {
    if (b1.click || b1.longPress || b1.pressed || b2.click) {
      idleMode_ = false;
      idleMs_ = 0;
    }
    if (b2.click) {
      requestExitToMenu();
    }
    return;
  }

  if (b1.click || b1.longPress || b1.pressed || b2.click) {
    idleMs_ = 0;
  }

  if (mode_ == Mode::ChoosePet) {
    if (b2.click) {
      requestExitToMenu();
      return;
    }
    if (b1.click) {
      petType_ = (petType_ + 1) % PET_COUNT;
    }
    if (b1.longPress) {
      resetCareStats();
      savePet();
      mode_ = Mode::Menu;
    }
    return;
  }

  if (mode_ == Mode::Dead) {
    if (b2.click) {
      savePet();
      requestExitToMenu();
      return;
    }
    if (b1.longPress) {
      resetCareStats();
      loaded_ = false;
      mode_ = Mode::ChoosePet;
    }
    return;
  }

  if (b2.click) {
    if (mode_ == Mode::Menu) {
      savePet();
      requestExitToMenu();
    } else {
      mode_ = Mode::Menu;
    }
    return;
  }

  if (mode_ == Mode::Stats) {
    if (b1.click || b1.longPress) {
      mode_ = Mode::Menu;
    }
    return;
  }

  if (mode_ == Mode::Message) {
    messageMs_ += deltaMs;
    if (messageMs_ > MESSAGE_DURATION_MS || b1.click || b1.longPress) {
      mode_ = Mode::Menu;
    }
    return;
  }

  if (mode_ == Mode::PlayScene) {
    if (b2.click) {
      mode_ = Mode::Menu;
      return;
    }
    updatePlayScene(deltaMs);
    return;
  }

  if (b1.click) {
    menuIndex_ = (menuIndex_ + 1) % ACTION_COUNT;
  }
  if (b1.longPress) {
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
  petX_ = constrain(static_cast<int>(petX_) + petDx_ * 2, 22, static_cast<int>(width) - 22);
  petY_ = constrain(static_cast<int>(petY_) + petDy_ * 2, 48, static_cast<int>(height) - 28);
}

template <typename Canvas>
void PetSimulatorApp::drawPet(Canvas& tft, int x, int y) {
  tft.setTextSize(1);
  if (petType_ == 0) {
    tft.drawCircle(x, y, 14, TFT_WHITE);
    tft.drawLine(x - 10, y - 10, x - 16, y - 22, TFT_WHITE);
    tft.drawLine(x - 16, y - 22, x - 4, y - 14, TFT_WHITE);
    tft.drawLine(x + 10, y - 10, x + 16, y - 22, TFT_WHITE);
    tft.drawLine(x + 16, y - 22, x + 4, y - 14, TFT_WHITE);
    tft.drawPixel(x - 5, y - 3, TFT_WHITE);
    tft.drawPixel(x + 5, y - 3, TFT_WHITE);
    tft.drawFastHLine(x - 4, y + 6, 8, TFT_WHITE);
  } else if (petType_ == 1) {
    tft.drawCircle(x, y, 14, TFT_WHITE);
    tft.fillRect(x - 20, y - 11, 8, 16, TFT_WHITE);
    tft.fillRect(x + 12, y - 11, 8, 16, TFT_WHITE);
    tft.drawPixel(x - 5, y - 3, TFT_WHITE);
    tft.drawPixel(x + 5, y - 3, TFT_WHITE);
    tft.drawFastHLine(x - 5, y + 7, 10, TFT_WHITE);
  } else if (petType_ == 2) {
    tft.drawTriangle(x - 18, y + 14, x + 20, y, x - 18, y - 16, TFT_WHITE);
    tft.drawPixel(x + 8, y - 3, TFT_WHITE);
    tft.drawLine(x - 8, y + 14, x - 12, y + 24, TFT_WHITE);
    tft.drawLine(x + 4, y + 8, x + 8, y + 20, TFT_WHITE);
  } else {
    tft.fillCircle(x, y, 15, TFT_DARKGREEN);
    tft.drawCircle(x, y, 15, TFT_WHITE);
    tft.drawPixel(x - 5, y - 4, TFT_WHITE);
    tft.drawPixel(x + 5, y - 4, TFT_WHITE);
    tft.drawFastHLine(x - 4, y + 7, 8, TFT_WHITE);
  }
}

template <typename Canvas>
void PetSimulatorApp::drawPoop(Canvas& tft, int x, int y) {
  tft.fillCircle(x, y + 4, 4, TFT_BROWN);
  tft.fillCircle(x + 4, y + 3, 3, TFT_BROWN);
  tft.fillCircle(x - 4, y + 5, 3, TFT_BROWN);
  tft.drawPixel(x, y + 2, TFT_YELLOW);
}

template <typename Canvas>
void PetSimulatorApp::drawPoops(Canvas& tft) {
  for (uint8_t i = 0; i < poop_; i++) {
    const int x = 28 + (i * 42) % static_cast<int>(width - 55);
    const int y = height - 32 - (i % 2) * 10;
    drawPoop(tft, x, y);
  }
}

template <typename Canvas>
void PetSimulatorApp::drawLowHealthWarning(Canvas& tft) {
  if (health_ == 0 || health_ > LOW_HEALTH_WARNING || ((millis() / 350) % 2) == 0) {
    return;
  }
  tft.drawRoundRect(4, 4, width - 8, height - 8, 7, TFT_RED);
  tft.setTextSize(1);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("LOW HP", width - 58, 12);
}

template <typename Canvas>
void PetSimulatorApp::drawBar(Canvas& tft, int x, int y, const char* label, uint8_t value) {
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(label, x, y);
  tft.drawRect(x + 42, y - 1, 120, 8, TFT_WHITE);
  const uint16_t color = value > 60 ? TFT_GREEN : value > 25 ? TFT_YELLOW : TFT_RED;
  tft.fillRect(x + 44, y + 1, map(value, 0, 100, 0, 116), 4, color);
}

void PetSimulatorApp::drawRunning(TFT_eSPI& tft) {
  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
  if (idleMode_) {
    drawPoops(canvas);
    drawPet(canvas, petX_, petY_);
    drawLowHealthWarning(canvas);
    drawFooter(canvas, width, height, "B1 wake  B2 menu");
    return;
  }

  if (mode_ == Mode::ChoosePet) {
    drawShell(canvas, width, height, "Choose Pet");
    drawPet(canvas, width / 2, 72);
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.drawString(PET_NAMES[petType_], 18, 104);
    drawFooter(canvas, width, height, "B1 next/open  B2 back");
    return;
  }

  if (mode_ == Mode::Dead) {
    drawShell(canvas, width, height, "Pet Simulator");
    canvas.setTextSize(4);
    canvas.setTextColor(TFT_RED, TFT_BLACK);
    canvas.drawString("RIP", 84, 48);
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString(PET_NAMES[petType_], 86, 88);
    drawFooter(canvas, width, height, "B1 hold new pet  B2 menu");
    return;
  }

  if (mode_ == Mode::PlayScene) {
    canvas.drawRect(0, 0, width, height, TFT_DARKGREY);
    drawPoops(canvas);
    canvas.fillCircle(toyX_, toyY_, 4, TFT_CYAN);
    canvas.drawCircle(toyX_, toyY_, 7, TFT_BLUE);
    drawPet(canvas, petX_, petY_);
    drawFooter(canvas, width, height, "Playing...");
    return;
  }

  if (mode_ == Mode::Stats) {
    drawShell(canvas, width, height, "Pet Stats");
    drawBar(canvas, 24, 47, "Hunger", hunger_);
    drawBar(canvas, 24, 63, "Fun", fun_);
    drawBar(canvas, 24, 79, "Energy", energy_);
    drawBar(canvas, 24, 95, "Clean", clean_);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setCursor(186, 79);
    canvas.print("HP ");
    canvas.print(health_);
    drawLowHealthWarning(canvas);
    drawFooter(canvas, width, height, "B1/B2 back");
    return;
  }

  if (mode_ == Mode::Message) {
    drawShell(canvas, width, height, PET_NAMES[petType_]);
    drawPoops(canvas);
    drawPet(canvas, 72, 76);
    canvas.setTextSize(3);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.drawString(message_, 126, 64);
    drawLowHealthWarning(canvas);
    drawFooter(canvas, width, height, "B1 continue  B2 back");
    return;
  }

  drawShell(canvas, width, height, PET_NAMES[petType_]);
  drawPoops(canvas);
  drawPet(canvas, 66, 74);
  canvas.setTextSize(2);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.drawString("Action", 126, 51);
  canvas.fillRect(122, 75, 94, 28, TFT_GREEN);
  canvas.setTextColor(TFT_BLACK, TFT_GREEN);
  canvas.drawString(ACTIONS[menuIndex_], 132, 82);
  drawLowHealthWarning(canvas);
  drawFooter(canvas, width, height, "B1 next/open  B2 menu");
  });
}
