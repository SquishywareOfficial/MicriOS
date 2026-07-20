#include "MiniLanderGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../CydFramebuffer.h"
#include "../../CydHardware.h"
#include "../../CydUi.h"

namespace {
constexpr int HUD_H = 28;
constexpr int GROUND_Y = 190;
constexpr int SHIP_X = 232;
constexpr int LANDER_FOOT_OFFSET = 13;
constexpr float THRUST = 48.0f;
constexpr float FUEL_BURN = 24.0f;
constexpr float BURN_CUE_REACTION_SEC = 0.35f;
constexpr uint16_t BRIEFING_INPUT_LOCK_MS = 1000;
Preferences landerPrefs;

template <typename Canvas>
void drawSafeSpeedSignOn(Canvas& canvas, int x, int y, int safeSpeed) {
  canvas.fillCircle(x, y, 18, TFT_WHITE);
  canvas.drawCircle(x, y, 17, TFT_RED);
  canvas.drawCircle(x, y, 16, TFT_RED);
  canvas.setTextSize(2);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setCursor(x - (safeSpeed >= 10 ? 11 : 5), y - 7);
  canvas.print(safeSpeed);
}

template <typename Canvas>
void drawBriefingOn(Canvas& canvas,
                    uint8_t level,
                    float startFuel,
                    float gravity,
                    float safeSpeed,
                    bool acceptingInput) {
  CydUi::header(canvas, "Landing Brief", TFT_CYAN, (String("L") + String(level)).c_str());
  CydUi::labelValue(canvas, 41, "Fuel", String(static_cast<int>(startFuel + 0.5f)), TFT_GREEN);
  CydUi::labelValue(canvas, 65, "Gravity", String(static_cast<int>(gravity + 0.5f)), TFT_YELLOW);
  CydUi::labelValue(canvas, 89, "Safe V", "<= " + String(static_cast<int>(safeSpeed + 0.5f)), TFT_CYAN);
  CydUi::footer(canvas, acceptingInput ? "Tap to start descent" : "Ready...");
}

template <typename Canvas>
void drawHudOn(Canvas& canvas,
               uint8_t level,
               bool thrusting,
               float altitude,
               float velocity,
               float fuel,
               float startFuel) {
  CydUi::header(canvas, "Mini Lander", thrusting ? TFT_ORANGE : TFT_CYAN, (String("L") + String(level)).c_str());
  canvas.setTextSize(2);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.drawString("A " + String(static_cast<int>(altitude + 0.5f)), 10, 34);
  canvas.drawString("V " + String(static_cast<int>(velocity + 0.5f)), 10, 56);
  const uint16_t fuelColor = fuel <= startFuel * 0.2f ? TFT_RED : TFT_GREEN;
  CydUi::bar(canvas, 10, 83, 104, 12, fuel / startFuel, fuelColor);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString("Fuel", 10, 96);
}

template <typename Canvas>
void drawLanderOn(Canvas& canvas, int x, int y, bool thrusting) {
  canvas.fillTriangle(x, y - 12, x - 9, y + 5, x + 9, y + 5, TFT_LIGHTGREY);
  canvas.drawTriangle(x, y - 12, x - 9, y + 5, x + 9, y + 5, TFT_WHITE);
  canvas.drawLine(x - 9, y + 5, x - 15, y + 13, TFT_WHITE);
  canvas.drawLine(x + 9, y + 5, x + 15, y + 13, TFT_WHITE);
  canvas.drawFastHLine(x - 18, y + 13, 11, TFT_WHITE);
  canvas.drawFastHLine(x + 7, y + 13, 11, TFT_WHITE);
  if (thrusting) {
    canvas.fillTriangle(x - 5, y + 7, x + 5, y + 7, x, y + 25, TFT_ORANGE);
    canvas.drawLine(x - 2, y + 8, x, y + 21, TFT_YELLOW);
    canvas.drawLine(x + 2, y + 8, x, y + 21, TFT_YELLOW);
  }
}

template <typename Canvas>
void drawGroundOn(Canvas& canvas, uint32_t width) {
  canvas.drawLine(1, GROUND_Y, width - 2, GROUND_Y, TFT_LIGHTGREY);
  canvas.fillRect(SHIP_X - 30, GROUND_Y - 2, 60, 4, TFT_GREEN);
  canvas.fillCircle(42, GROUND_Y - 7, 3, TFT_DARKGREY);
  canvas.fillCircle(292, GROUND_Y - 11, 5, TFT_DARKGREY);
}
}

MiniLanderGame::MiniLanderGame(uint32_t width, uint32_t height)
    : App("Mini Lander", width, height) {}

bool MiniLanderGame::hasCustomOverlay() const {
  return true;
}

void MiniLanderGame::onAppReset() {
  setBurnHintLed(false);
  loadBestLevel();
  level_ = 1;
  crashed_ = false;
  loadLevel();
}

void MiniLanderGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)b2;
  const float deltaSec = static_cast<float>(deltaMs) * 0.001f;

  if (landerState_ == LanderState::Briefing) {
    setBurnHintLed(false);
    briefingTimerMs_ += deltaMs;
    if (briefingTimerMs_ < BRIEFING_INPUT_LOCK_MS) {
      return;
    }
    if (!briefingCanAcceptInput_) {
      if (!b1.down) {
        briefingCanAcceptInput_ = true;
      }
      return;
    }
    if (b1.click) {
      landerState_ = LanderState::Falling;
    }
    return;
  }

  if (landerState_ == LanderState::Landed) {
    setBurnHintLed(false);
    landedTimerMs_ += deltaMs;
    if (landedTimerMs_ >= 900) {
      level_++;
      loadLevel();
    }
    return;
  }

  const bool thrusting = b1.down && fuel_ > 0.0f;
  thrusting_ = thrusting;
  const bool lowFuelBlink = fuel_ <= (startFuel_ * 0.2f) && ((millis() / 180) % 2) == 0;
  setBurnHintLed((shouldShowBurnHint() && !thrusting) || lowFuelBlink);
  float accel = levelGravity();
  if (thrusting) {
    accel -= THRUST;
    fuel_ -= FUEL_BURN * deltaSec;
    if (fuel_ < 0.0f) {
      fuel_ = 0.0f;
    }
  }

  velocity_ += accel * deltaSec;
  altitude_ -= velocity_ * deltaSec;

  if (altitude_ <= 0.0f) {
    altitude_ = 0.0f;
    if (velocity_ <= levelSafeSpeed()) {
      velocity_ = 0.0f;
      landerState_ = LanderState::Landed;
      landedTimerMs_ = 0;
      recordLandedLevel();
      setBurnHintLed(false);
    } else {
      crashed_ = true;
      setBurnHintLed(false);
      endApp();
    }
  }
}

void MiniLanderGame::drawRunning(TFT_eSPI& tft) {
  CydFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    CydUi::clear(canvas);
    if (landerState_ == LanderState::Briefing) {
      const bool acceptingInput = briefingTimerMs_ >= BRIEFING_INPUT_LOCK_MS && briefingCanAcceptInput_;
      drawBriefingOn(canvas, level_, startFuel_, levelGravity(), levelSafeSpeed(), acceptingInput);
      return;
    }

    drawHudOn(canvas, level_, thrusting_, altitude_, velocity_, fuel_, startFuel_);
    drawGroundOn(canvas, width);

    const float altitudeRatio = altitude_ / startAltitude_;
    const int altitudePixels = altitude_ <= 0.0f ? 0 : max(1, static_cast<int>(ceilf(altitudeRatio * 137.0f)));
    const int shipY = GROUND_Y - LANDER_FOOT_OFFSET - altitudePixels;
    drawLanderOn(canvas, SHIP_X, shipY, thrusting_);

    if (landerState_ == LanderState::Landed) {
      CydUi::centered(canvas, "LANDED", 54, 4, TFT_GREEN);
    }
  });
}

void MiniLanderGame::drawStart(TFT_eSPI& tft) { CydUi::clear(tft);
  CydUi::clear(tft);
  if (showStartPromptPage()) {
    CydUi::header(tft, "Mini Lander", TFT_CYAN);
    CydUi::centered(tft, "Press to Start", 56, 2, TFT_WHITE);
    CydUi::footer(tft, "Hold screen for thrust");
    return;
  }
  if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    CydUi::header(tft, "Best Level", TFT_CYAN);
    String score = bestLevel_ == 0 ? "--" : String(initials) + " L" + String(bestLevel_);
    CydUi::largeValue(tft, score, 54, TFT_CYAN);
    CydUi::footer(tft, "Highest landing level");
    return;
  }
  CydUi::header(tft, "Mini Lander", TFT_CYAN);
  tft.fillCircle(82, 103, 48, TFT_DARKGREY);
  tft.drawCircle(82, 103, 48, TFT_LIGHTGREY);
  tft.drawCircle(62, 86, 8, TFT_BLACK);
  tft.drawCircle(96, 121, 11, TFT_BLACK);
  tft.drawCircle(109, 80, 5, TFT_BLACK);
  tft.drawLine(194, 68, 224, 82, TFT_WHITE);
  tft.drawTriangle(230, 83, 252, 72, 244, 101, TFT_WHITE);
  tft.drawPixel(183, 50, TFT_WHITE);
  tft.drawPixel(278, 120, TFT_WHITE);
  tft.drawPixel(232, 151, TFT_WHITE);
  CydUi::footer(tft, "Time the suicide burn");
}

void MiniLanderGame::drawEnd(TFT_eSPI& tft) { CydUi::clear(tft);
  CydUi::clear(tft);
  CydUi::header(tft, crashed_ ? "Crashed" : "Mission End", crashed_ ? TFT_RED : TFT_GREEN);
  CydUi::labelValue(tft, 45, "Level", String(level_), TFT_YELLOW);
  CydUi::labelValue(tft, 72, "Velocity", String(static_cast<int>(velocity_ + 0.5f)), crashed_ ? TFT_RED : TFT_GREEN);
  drawSafeSpeedSign(tft, 196, 73, static_cast<int>(levelSafeSpeed() + 0.5f));
  CydUi::footer(tft, "Tap Play Again");
}

void MiniLanderGame::loadBestLevel() {
  if (bestLoaded_) {
    return;
  }
  landerPrefs.begin("lander", true);
  bestLevel_ = landerPrefs.getUChar("best", 0);
  bestInitials_ = landerPrefs.getUShort("init", PlayerProfile::defaultInitials());
  landerPrefs.end();
  bestLoaded_ = true;
}

void MiniLanderGame::saveBestLevel() {
  bestInitials_ = PlayerProfile::loadInitials();
  landerPrefs.begin("lander", false);
  landerPrefs.putUChar("best", bestLevel_);
  landerPrefs.putUShort("init", bestInitials_);
  landerPrefs.end();
}

void MiniLanderGame::recordLandedLevel() {
  loadBestLevel();
  if (level_ > bestLevel_) {
    bestLevel_ = level_;
    saveBestLevel();
  }
}

void MiniLanderGame::loadLevel() {
  landerState_ = LanderState::Briefing;
  briefingTimerMs_ = 0;
  briefingCanAcceptInput_ = false;
  landedTimerMs_ = 0;
  startAltitude_ = 90.0f + static_cast<float>(level_ - 1) * 8.0f;
  altitude_ = startAltitude_;
  velocity_ = 3.5f + static_cast<float>(level_ - 1) * 1.6f;
  startFuel_ = max(22.0f, 70.0f - static_cast<float>(level_ - 1) * 6.0f);
  fuel_ = startFuel_;
  thrusting_ = false;
  setBurnHintLed(false);
}

float MiniLanderGame::levelGravity() const {
  if (level_ <= 3) {
    return 6.5f + static_cast<float>(level_ - 1) * 1.4f;
  }
  return 11.5f + static_cast<float>(level_ - 4) * 2.1f;
}

float MiniLanderGame::levelSafeSpeed() const {
  if (level_ == 1) {
    return 14.0f;
  }
  if (level_ == 2) {
    return 13.0f;
  }
  if (level_ == 3) {
    return 12.0f;
  }
  return max(8.0f, 12.0f - static_cast<float>(level_ - 3));
}

bool MiniLanderGame::shouldShowBurnHint() const {
  if (level_ > 3 || landerState_ != LanderState::Falling || fuel_ <= 0.0f) {
    return false;
  }
  const float netDecel = THRUST - levelGravity();
  if (netDecel <= 0.0f || velocity_ <= 1.0f) {
    return false;
  }
  const float gravity = levelGravity();
  const float reactionDistance =
      (velocity_ * BURN_CUE_REACTION_SEC) + (0.5f * gravity * BURN_CUE_REACTION_SEC * BURN_CUE_REACTION_SEC);
  const float velocityAfterReaction = velocity_ + (gravity * BURN_CUE_REACTION_SEC);
  const float stoppingDistance = (velocityAfterReaction * velocityAfterReaction) / (2.0f * netDecel);
  const float cueError = altitude_ - (reactionDistance + stoppingDistance);
  return cueError <= 3.0f && cueError >= -2.0f;
}

void MiniLanderGame::setBurnHintLed(bool on) {
  CydHardware::setRgb(0, on ? 90 : 0, on ? 255 : 0);
}

void MiniLanderGame::onAppExit() { setBurnHintLed(false); }

void MiniLanderGame::drawSafeSpeedSign(TFT_eSPI& tft, int x, int y, int safeSpeed) {
  drawSafeSpeedSignOn(tft, x, y, safeSpeed);
}

void MiniLanderGame::drawBriefing(TFT_eSPI& tft) {
  const bool acceptingInput = briefingTimerMs_ >= BRIEFING_INPUT_LOCK_MS && briefingCanAcceptInput_;
  drawBriefingOn(tft, level_, startFuel_, levelGravity(), levelSafeSpeed(), acceptingInput);
}

void MiniLanderGame::drawHud(TFT_eSPI& tft) {
  drawHudOn(tft, level_, thrusting_, altitude_, velocity_, fuel_, startFuel_);
}

void MiniLanderGame::drawLander(TFT_eSPI& tft, int x, int y, bool thrusting) {
  drawLanderOn(tft, x, y, thrusting);
}

void MiniLanderGame::drawGround(TFT_eSPI& tft) {
  drawGroundOn(tft, width);
}
