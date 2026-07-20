#include "FishingFlickGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <math.h>

#include "../../PlayerProfile.h"
#include "../../CydFramebuffer.h"
#include "../../CydHardware.h"
#include "../../CydUi.h"

namespace {
Preferences fishPrefs;

constexpr int16_t ARENA_CX = 104;
constexpr int16_t ARENA_CY = 110;
constexpr int16_t ARENA_R = 68;
constexpr uint8_t NO_QUADRANT = 0xFF;
constexpr uint16_t WATER_DEEP = 0x0012;
constexpr uint16_t WATER_MID = 0x021B;
constexpr uint16_t SKY_DUSK = 0x18B5;
constexpr uint16_t PULL_IDLE = 0x0328;
constexpr uint16_t PULL_GOOD = 0x03E0;
constexpr uint16_t PULL_BAD = 0x7800;

uint8_t oppositeQuadrant(uint8_t quadrant) {
  return static_cast<uint8_t>((quadrant + 2) & 0x03);
}

template <typename Canvas>
void drawFish(Canvas& canvas, int16_t x, int16_t y, bool facingRight,
              uint16_t color, uint8_t scale = 1) {
  const int16_t bodyW = 9 * scale;
  const int16_t bodyH = 5 * scale;
  canvas.fillEllipse(x, y, bodyW, bodyH, color);
  const int16_t tailX = facingRight ? x - bodyW : x + bodyW;
  const int16_t tipX = facingRight ? tailX - 8 * scale : tailX + 8 * scale;
  canvas.fillTriangle(tailX, y, tipX, y - 6 * scale, tipX,
                      y + 6 * scale, color);
  const int16_t eyeX = facingRight ? x + 4 * scale : x - 4 * scale;
  canvas.fillCircle(eyeX, y - 2 * scale, scale == 0 ? 1 : scale, TFT_WHITE);
}

template <typename Canvas>
void drawLakeScene(Canvas& canvas, bool tugging, bool splash) {
  canvas.fillRect(0, CydUi::HEADER_H, CydUi::W,
                  CydUi::H - CydUi::HEADER_H, SKY_DUSK);
  canvas.fillRect(0, 61, CydUi::W, 51, TFT_NAVY);
  canvas.fillCircle(261, 66, 20, TFT_ORANGE);
  canvas.fillCircle(261, 66, 14, TFT_YELLOW);

  canvas.fillTriangle(0, 112, 62, 63, 126, 112, TFT_DARKGREY);
  canvas.fillTriangle(76, 112, 145, 69, 213, 112, 0x3186);
  canvas.fillTriangle(176, 112, 242, 75, 319, 112, TFT_DARKGREY);
  canvas.drawLine(48, 74, 62, 63, TFT_LIGHTGREY);
  canvas.drawLine(62, 63, 77, 76, TFT_LIGHTGREY);

  canvas.fillRect(0, 112, CydUi::W, CydUi::H - 112, WATER_DEEP);
  canvas.fillRect(0, 112, CydUi::W, 18, WATER_MID);
  for (uint8_t i = 0; i < 9; ++i) {
    const int16_t x = 8 + i * 38;
    canvas.drawFastHLine(x, 121 + (i & 1) * 7, 20, TFT_CYAN);
    canvas.drawFastHLine(x + 8, 149 + (i % 3) * 8, 26, TFT_BLUE);
  }

  // Jetty and angler silhouette.
  canvas.fillRect(0, 118, 88, 9, TFT_BROWN);
  canvas.fillRect(17, 126, 7, 58, TFT_BROWN);
  canvas.fillRect(72, 126, 7, 58, TFT_BROWN);
  canvas.fillCircle(64, 78, 8, TFT_ORANGE);
  canvas.drawLine(64, 86, 64, 112, TFT_WHITE);
  canvas.drawLine(64, 95, 79, 105, TFT_WHITE);
  canvas.drawLine(64, 111, 49, 120, TFT_WHITE);
  canvas.drawLine(64, 111, 76, 120, TFT_WHITE);

  const int16_t bobX = tugging ? 256 : 248;
  const int16_t bobY = tugging ? 132 : 124;
  canvas.drawLine(79, 104, 139, 61, TFT_ORANGE);
  canvas.drawLine(80, 105, 140, 62, TFT_YELLOW);
  canvas.drawLine(140, 62, 215, 88, TFT_YELLOW);
  canvas.drawLine(215, 88, bobX, bobY, TFT_GREEN);
  canvas.fillCircle(bobX, bobY, 4, tugging ? TFT_RED : TFT_WHITE);
  canvas.drawEllipse(bobX, bobY + 4, tugging ? 24 : 14, 5, TFT_CYAN);

  if (tugging) {
    drawFish(canvas, 277, 153, false, TFT_MAGENTA, 1);
    canvas.drawLine(bobX, bobY, 270, 149, TFT_GREEN);
    canvas.drawLine(270, 149, 277, 153, TFT_GREEN);
  } else if (splash) {
    drawFish(canvas, 268, 146, false, TFT_MAGENTA, 2);
    canvas.drawArc(268, 142, 24, 19, 205, 330, TFT_CYAN, WATER_DEEP);
  }
}

template <typename Canvas>
void fillQuadrant(Canvas& canvas, uint8_t quadrant, uint16_t color) {
  static const int16_t startDegrees[4] = {180, 270, 0, 90};
  const int16_t start = startDegrees[quadrant & 0x03];
  for (int16_t degree = start; degree < start + 90; degree += 15) {
    const float a0 = static_cast<float>(degree) * 0.0174532925f;
    const float a1 = static_cast<float>(degree + 15) * 0.0174532925f;
    canvas.fillTriangle(ARENA_CX, ARENA_CY,
                        ARENA_CX + static_cast<int16_t>(cosf(a0) * (ARENA_R - 3)),
                        ARENA_CY + static_cast<int16_t>(sinf(a0) * (ARENA_R - 3)),
                        ARENA_CX + static_cast<int16_t>(cosf(a1) * (ARENA_R - 3)),
                        ARENA_CY + static_cast<int16_t>(sinf(a1) * (ARENA_R - 3)),
                        color);
  }
}

}

FishingFlickGame::FishingFlickGame(uint32_t width, uint32_t height)
    : App("Fishing Flick", width, height) {}

bool FishingFlickGame::hasCustomOverlay() const {
  return true;
}

bool FishingFlickGame::handleTouch(const TouchUi::TouchSample& sample,
                                   const TouchUi::TouchEvent& event) {
  if (phase() != AppPhase::Running || fishState_ != FishState::Reeling) {
    quadrantTouchDown_ = false;
    touchedQuadrant_ = NO_QUADRANT;
    return App::handleTouch(sample, event);
  }

  const TouchUi::Point point = TouchUi::currentPoint(sample, event);
  touchedQuadrant_ = sample.down ? static_cast<uint8_t>(quadrantAt(point))
                                 : NO_QUADRANT;
  quadrantTouchDown_ = sample.down && touchedQuadrant_ != NO_QUADRANT;
  if (event.tap) {
    const int8_t tapped = quadrantAt(event.point);
    if (tapped >= 0) {
      tappedQuadrant_ = static_cast<uint8_t>(tapped);
      quadrantTapPending_ = true;
    }
  }

  // Reeling is entirely direct-touch. Consuming every active touch prevents
  // the legacy full-screen B1 mapping from also firing underneath it.
  return sample.down || event.pressed || event.released || event.tap;
}

void FishingFlickGame::onAppReset() {
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
  fishMoveTimerMs_ = static_cast<uint16_t>(random(800, 1300));
  caughtTimerMs_ = 0;
  fishWeight_ = static_cast<uint16_t>(180 + (level_ * 120) + random(0, 140 + level_ * 25));
  progress_ = 0.0f;
  tension_ = 34.0f;
  fighting_ = false;
  quadrantTouchDown_ = false;
  quadrantTapPending_ = false;
  touchedQuadrant_ = NO_QUADRANT;
  tappedQuadrant_ = NO_QUADRANT;
  fishQuadrant_ = static_cast<uint8_t>(random(0, 4));
  setWarningLed(false);
}

void FishingFlickGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)b2;
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
    if (b1.pressed) {
      fishState_ = FishState::Reeling;
      tension_ = 36.0f;
      progress_ = 6.0f;
      fighting_ = false;
      fightTimerMs_ = 650;
      fishMoveTimerMs_ = 850;
      moveFish();
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
  (void)b1;

  if (fishMoveTimerMs_ > deltaMs) {
    fishMoveTimerMs_ -= deltaMs;
  } else {
    moveFish();
    fishMoveTimerMs_ = static_cast<uint16_t>(
        random(fighting_ ? 480 : 760, fighting_ ? 920 : 1450));
    tension_ += fighting_ ? 3.5f : 1.5f;
  }

  const uint8_t counterQuadrant = oppositeQuadrant(fishQuadrant_);
  const bool correctPull = quadrantTouchDown_ &&
                           touchedQuadrant_ == counterQuadrant;
  const bool wrongPull = quadrantTouchDown_ && !correctPull;

  if (correctPull) {
    progress_ += (fighting_ ? (14.0f / hard) : (30.0f / hard)) * deltaSec;
    tension_ += (fighting_ ? (19.0f + hard * 4.0f)
                           : (7.0f + hard)) * deltaSec;
  } else if (wrongPull) {
    progress_ -= (fighting_ ? 10.0f : 5.0f) * deltaSec;
    tension_ += (fighting_ ? (58.0f + hard * 8.0f)
                           : (40.0f + hard * 6.0f)) * deltaSec;
  } else {
    progress_ -= (fighting_ ? 3.0f : 1.0f) * deltaSec;
    tension_ -= (fighting_ ? (34.0f + hard * 4.0f)
                           : (22.0f + hard * 3.0f)) * deltaSec;
  }

  if (quadrantTapPending_) {
    if (tappedQuadrant_ == counterQuadrant) {
      progress_ += 1.8f / hard;
      tension_ += 0.8f;
    } else {
      progress_ -= 0.7f;
      tension_ += 4.0f + hard;
    }
    quadrantTapPending_ = false;
    tappedQuadrant_ = NO_QUADRANT;
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

void FishingFlickGame::drawRunning(TFT_eSPI& tft) {
  CydFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    CydUi::clear(canvas);
    const String levelText = String("LEVEL ") + String(level_);
    CydUi::header(canvas, "Fishing Flick", TFT_CYAN, levelText.c_str());

    if (fishState_ == FishState::Waiting) {
      drawLakeScene(canvas, false, false);
      CydUi::centered(canvas, "Watch the float...", 41, 2, TFT_WHITE);
      CydUi::footer(canvas, "Wait for the tug");
      return;
    }

    if (fishState_ == FishState::Hooking) {
      drawLakeScene(canvas, true, false);
      CydUi::centered(canvas, "TUG!", 42, 4, TFT_YELLOW);
      CydUi::touchAction(canvas, {98, 145, 124, 32}, "HOOK!", TFT_ORANGE);
      CydUi::footer(canvas, "Tap anywhere quickly");
      return;
    }

    if (fishState_ == FishState::Caught) {
      CydUi::centered(canvas, "CAUGHT!", 38, 4, TFT_GREEN);
      CydUi::largeValue(canvas, String(caughtWeight_) + "g", 79, TFT_CYAN);
      CydUi::footer(canvas, "Next fish...");
      return;
    }

    // Direct-touch reel arena. The highlighted quadrant is diagonally
    // opposite the fish and therefore the effective counter-pull direction.
    const uint8_t counterQuadrant = oppositeQuadrant(fishQuadrant_);
    canvas.fillCircle(ARENA_CX, ARENA_CY, ARENA_R, WATER_DEEP);
    fillQuadrant(canvas, counterQuadrant, PULL_IDLE);
    if (quadrantTouchDown_ && touchedQuadrant_ != NO_QUADRANT) {
      fillQuadrant(canvas, touchedQuadrant_,
                   touchedQuadrant_ == counterQuadrant ? PULL_GOOD : PULL_BAD);
    }
    canvas.drawCircle(ARENA_CX, ARENA_CY, ARENA_R, TFT_CYAN);
    canvas.drawCircle(ARENA_CX, ARENA_CY, ARENA_R - 1, TFT_BLUE);
    canvas.drawFastHLine(ARENA_CX - ARENA_R, ARENA_CY, ARENA_R * 2,
                         TFT_DARKGREY);
    canvas.drawFastVLine(ARENA_CX, ARENA_CY - ARENA_R, ARENA_R * 2,
                         TFT_DARKGREY);

    // Rod ends at the centre; the live green line runs from its tip to fish.
    canvas.drawLine(ARENA_CX - 54, ARENA_CY + 56, ARENA_CX - 19,
                    ARENA_CY + 20, TFT_BROWN);
    canvas.drawLine(ARENA_CX - 53, ARENA_CY + 56, ARENA_CX - 18,
                    ARENA_CY + 20, TFT_ORANGE);
    canvas.drawLine(ARENA_CX - 19, ARENA_CY + 20, ARENA_CX, ARENA_CY,
                    TFT_ORANGE);

    static const float quadrantAngles[4] = {225.0f, 315.0f, 45.0f, 135.0f};
    const float wobble = sinf(static_cast<float>(millis()) *
                              (fighting_ ? 0.012f : 0.006f)) *
                         (fighting_ ? 13.0f : 7.0f);
    const float fishAngle = (quadrantAngles[fishQuadrant_] + wobble) *
                            0.0174532925f;
    const int16_t fishX = ARENA_CX + static_cast<int16_t>(cosf(fishAngle) * 48.0f);
    const int16_t fishY = ARENA_CY + static_cast<int16_t>(sinf(fishAngle) * 48.0f);
    canvas.drawLine(ARENA_CX, ARENA_CY, fishX, fishY, TFT_GREEN);
    canvas.drawLine(ARENA_CX + 1, ARENA_CY, fishX + 1, fishY, TFT_GREEN);
    drawFish(canvas, fishX, fishY, fishX > ARENA_CX,
             fighting_ ? TFT_MAGENTA : TFT_PURPLE, 1);

    static const int16_t pullX[4] = {63, 123, 123, 63};
    static const int16_t pullY[4] = {70, 70, 148, 148};
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("PULL", pullX[counterQuadrant], pullY[counterQuadrant]);

    // Compact status dashboard leaves the majority of the screen to the reel.
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("MODE", 208, 45);
    canvas.setTextSize(2);
    canvas.setTextColor(fighting_ ? TFT_RED : TFT_GREEN, TFT_BLACK);
    canvas.drawString(fighting_ ? "FIGHT" : "REEL", 208, 57);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("FISH", 208, 84);
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas.drawString(String(fishWeight_) + "g", 208, 96);

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("CATCH", 208, 125);
    CydUi::bar(canvas, 208, 137, 99, 10, progress_ / 100.0f, TFT_GREEN);
    canvas.drawString("LINE", 208, 154);
    const uint16_t lineColor = tension_ > 78.0f
                                   ? TFT_RED
                                   : (tension_ < 15.0f ? TFT_CYAN : TFT_YELLOW);
    CydUi::bar(canvas, 208, 166, 99, 10, tension_ / 100.0f, lineColor);

    if (quadrantTouchDown_ && touchedQuadrant_ != counterQuadrant) {
      CydUi::footer(canvas, "Wrong side - line strain!");
    } else if (tension_ > 78.0f) {
      CydUi::footer(canvas, "High tension - release briefly");
    } else if (tension_ < 15.0f) {
      CydUi::footer(canvas, "Line slack - pull opposite fish");
    } else if (quadrantTouchDown_) {
      CydUi::footer(canvas, "Good pull - keep the line balanced");
    } else {
      CydUi::footer(canvas, "Hold the highlighted opposite quadrant");
    }
  });
}

void FishingFlickGame::drawStart(TFT_eSPI& tft) {
  loadBestFish();
  CydFramebuffer::draw(tft, static_cast<int16_t>(width),
                       static_cast<int16_t>(height), [&](auto& canvas) {
    CydUi::clear(canvas);
    if (showStartScorePage()) {
      CydUi::header(canvas, "Top Fish", TFT_CYAN);
      drawLakeScene(canvas, false, true);
      char initials[4];
      PlayerProfile::unpackDottedInitials(bestInitials_, initials);
      const String score = bestWeight_ == 0
                               ? "--"
                               : String(initials) + " " + String(bestWeight_) + "g";
      canvas.fillRoundRect(78, 47, 164, 58, 8, TFT_BLACK);
      canvas.drawRoundRect(78, 47, 164, 58, 8, TFT_CYAN);
      CydUi::largeValue(canvas, score, 59, TFT_CYAN);
      CydUi::footer(canvas, "Heaviest fish landed");
    } else {
      CydUi::header(canvas, "Fishing Flick", TFT_CYAN,
                    showStartPromptPage() ? "READY" : "LAKE CHALLENGE");
      drawLakeScene(canvas, false, true);
      canvas.fillRoundRect(91, 40, 138, 31, 7, TFT_BLACK);
      canvas.drawRoundRect(91, 40, 138, 31, 7, TFT_CYAN);
      CydUi::centered(canvas,
                      showStartPromptPage() ? "Land the catch" : "CAST. HOOK. FIGHT.",
                      49, showStartPromptPage() ? 2 : 1, TFT_WHITE);
      CydUi::footer(canvas, "Counter the fish, protect the line");
    }
  });
}

void FishingFlickGame::drawEnd(TFT_eSPI& tft) { CydUi::clear(tft);
  setWarningLed(false);
  const char* title = fishState_ == FishState::Escaped ? "Escaped" : "Line broke";
  CydUi::clear(tft);
  CydUi::header(tft, title, fishState_ == FishState::Escaped ? TFT_CYAN : TFT_RED);
  String fish = "--";
  if (caughtWeight_ == 0) {
    fish = "--";
  } else {
    fish = String(caughtWeight_) + "g";
  }
  String best = "--";
  if (bestWeight_ == 0) {
    best = "--";
  } else {
    best = String(bestWeight_) + "g";
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    best += " ";
    best += initials;
  }
  CydUi::labelValue(tft, 49, "Fish", fish, TFT_YELLOW);
  CydUi::labelValue(tft, 78, "Best", best, TFT_GREEN);
  CydUi::footer(tft, "Tap Play Again");
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
  CydHardware::setRgb(on ? 255 : 0, on ? 36 : 0, 0);
}

void FishingFlickGame::moveFish() {
  fishQuadrant_ = static_cast<uint8_t>(
      (fishQuadrant_ + static_cast<uint8_t>(random(1, 4))) & 0x03);
}

int8_t FishingFlickGame::quadrantAt(TouchUi::Point point) const {
  const int32_t dx = static_cast<int32_t>(point.x) - ARENA_CX;
  const int32_t dy = static_cast<int32_t>(point.y) - ARENA_CY;
  if (dx * dx + dy * dy > static_cast<int32_t>(ARENA_R) * ARENA_R) {
    return -1;
  }
  if (dy < 0) return dx < 0 ? 0 : 1;
  return dx >= 0 ? 2 : 3;
}

void FishingFlickGame::onAppExit() { setWarningLed(false); }

void FishingFlickGame::updateWarningLed() {
  if (tension_ < 80.0f) {
    setWarningLed(false);
    return;
  }
  setWarningLed(((millis() / 120) % 2) == 0);
}
