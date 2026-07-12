#include "CityRacerGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
namespace {
Preferences cityRacerPrefs;
bool cityRacerFrameLogged = false;
uint32_t lastSpriteFallbackLogMs = 0;

constexpr uint8_t PORTRAIT_ROTATION = 0;
constexpr int PORTRAIT_W = 135;
constexpr int PORTRAIT_H = 240;
constexpr int HUD_H = 27;
constexpr int PLAYER_Y = 196;
constexpr int ROAD_X = 12;
constexpr int ROAD_W = 111;
constexpr int LANE_W = ROAD_W / 3;
constexpr int CAR_W = 22;
constexpr int CAR_H = 30;
constexpr uint16_t RUNNING_DRAW_MS = 33;

void setPortrait(TFT_eSPI& tft) {
  tft.setRotation(PORTRAIT_ROTATION);
}

void clearPortrait(TFT_eSPI& tft) {
  setPortrait(tft);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
}

template <typename Canvas>
void clearCanvas(Canvas& canvas) {
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextFont(1);
}

template <typename Canvas>
void drawPortraitHeader(Canvas& tft, const char* title, uint16_t color, const String& stat = String()) {
  tft.fillRect(0, 0, PORTRAIT_W, HUD_H, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(title, 6, 6);
  if (stat.length() > 0) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(stat, PORTRAIT_W - tft.textWidth(stat) - 6, 6);
  }
  tft.drawFastHLine(0, HUD_H - 1, PORTRAIT_W, TFT_DARKGREY);
}

template <typename Canvas>
void drawPortraitFooter(Canvas& tft, const char* text) {
  tft.fillRect(0, PORTRAIT_H - 17, PORTRAIT_W, 17, TFT_BLACK);
  tft.drawFastHLine(0, PORTRAIT_H - 18, PORTRAIT_W, TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(text, 6, PORTRAIT_H - 13);
}

template <typename Canvas>
void drawCentered(Canvas& tft, const String& text, int y, uint8_t size, uint16_t color) {
  tft.setTextSize(size);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(text, (PORTRAIT_W - tft.textWidth(text)) / 2, y);
}

template <typename Canvas>
void drawCarShape(Canvas& tft, int x, int y, bool player) {
  if (player) {
    tft.fillRoundRect(x, y, CAR_W, CAR_H, 3, TFT_CYAN);
    tft.fillRect(x + 5, y + 5, CAR_W - 10, 7, TFT_WHITE);
    tft.fillRect(x + 3, y + 4, 3, 6, TFT_BLACK);
    tft.fillRect(x + CAR_W - 6, y + 4, 3, 6, TFT_BLACK);
    tft.fillRect(x + 3, y + CAR_H - 9, 3, 6, TFT_BLACK);
    tft.fillRect(x + CAR_W - 6, y + CAR_H - 9, 3, 6, TFT_BLACK);
  } else {
    tft.fillRoundRect(x, y, CAR_W, CAR_H, 3, TFT_RED);
    tft.fillRect(x + 5, y + CAR_H - 12, CAR_W - 10, 7, TFT_ORANGE);
    tft.fillRect(x + 3, y + 4, 3, 6, TFT_BLACK);
    tft.fillRect(x + CAR_W - 6, y + 4, 3, 6, TFT_BLACK);
    tft.fillRect(x + 3, y + CAR_H - 9, 3, 6, TFT_BLACK);
    tft.fillRect(x + CAR_W - 6, y + CAR_H - 9, 3, 6, TFT_BLACK);
  }
}

}

CityRacerGame::CityRacerGame(uint32_t width, uint32_t height)
    : App("City Racer", width, height) {}

bool CityRacerGame::hasCustomOverlay() const {
  return true;
}

void CityRacerGame::onAppReset() {
  loadBestScore();
  playerLane_ = 1;
  plannedSafeLane_ = playerLane_;
  rowsSinceLaneChange_ = 0;
  level_ = 1;
  score_ = 0;
  speed_ = 138.0f;
  spawnIntervalMs_ = 780;
  spawnTimerMs_ = 360;
  for (uint8_t i = 0; i < ROWS; i++) rows_[i].active = false;
}

uint16_t CityRacerGame::runningRenderIntervalMs() const {
  return RUNNING_DRAW_MS;
}

void CityRacerGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  const float dt = static_cast<float>(deltaMs) * 0.001f;
  if (b1.click) playerLane_ = (playerLane_ + LANES - 1) % LANES;
  if (b2.click) playerLane_ = (playerLane_ + 1) % LANES;

  level_ = 1 + min<uint16_t>(9, score_ / 35);
  speed_ = 138.0f + static_cast<float>(min<uint8_t>(level_ - 1, 7)) * 13.0f;
  spawnIntervalMs_ = 780 - min<uint16_t>(230, max<int>(0, level_ - 1) * 24);

  spawnTimerMs_ += deltaMs;
  if (spawnTimerMs_ >= spawnIntervalMs_) {
    spawnTimerMs_ = 0;
    spawnRow();
  }

  for (uint8_t i = 0; i < ROWS; i++) {
    if (!rows_[i].active) continue;
    rows_[i].y += speed_ * dt;
    if (!rows_[i].counted && rows_[i].y > PLAYER_Y + CAR_H) {
      rows_[i].counted = true;
      for (uint8_t lane = 0; lane < LANES; lane++) {
        if (rows_[i].mask & (1 << lane)) score_++;
      }
      if (score_ > bestScore_) {
        bestScore_ = score_;
        saveBestScore();
      }
    }
    if (rows_[i].y > PORTRAIT_H + CAR_H) rows_[i].active = false;
    if (rows_[i].y >= PLAYER_Y - CAR_H && rows_[i].y <= PLAYER_Y + CAR_H && (rows_[i].mask & (1 << playerLane_))) {
      endApp();
      return;
    }
  }
}

uint8_t CityRacerGame::chooseSafeLane() {
  if (rowsSinceLaneChange_ == 0) {
    rowsSinceLaneChange_++;
    return plannedSafeLane_;
  }
  const uint8_t choice = random(0, 100);
  if (rowsSinceLaneChange_ < 3 && choice < 52) {
    rowsSinceLaneChange_++;
    return plannedSafeLane_;
  }
  rowsSinceLaneChange_ = 0;
  return (plannedSafeLane_ + 1) % LANES;
}

void CityRacerGame::spawnRow() {
  for (uint8_t i = 0; i < ROWS; i++) {
    if (rows_[i].active && rows_[i].y < HUD_H + 46.0f) return;
  }
  for (uint8_t i = 0; i < ROWS; i++) {
    if (rows_[i].active) continue;
    plannedSafeLane_ = chooseSafeLane();
    uint8_t mask = 0;
    const uint8_t firstBlocked = (plannedSafeLane_ + (random(0, 100) < 55 ? 1 : 2)) % LANES;
    mask |= (1 << firstBlocked);
    const bool addSecondCar = level_ >= 3 && random(0, 100) < min<uint8_t>(48, 18 + (level_ - 3) * 5);
    if (addSecondCar) {
      const uint8_t secondBlocked = (plannedSafeLane_ + 3 - (firstBlocked - plannedSafeLane_ + 3) % 3) % LANES;
      mask |= (1 << secondBlocked);
    }
    rows_[i].active = true;
    rows_[i].counted = false;
    rows_[i].y = HUD_H - CAR_H - 4;
    rows_[i].mask = mask;
    return;
  }
}

void CityRacerGame::drawRunning(TFT_eSPI& tft) {
  setPortrait(tft);

  auto drawScene = [this](auto& canvas) {
    clearCanvas(canvas);
    drawPortraitHeader(canvas, "City Racer", TFT_BLUE, String("L") + String(level_) + " C" + String(score_));
    canvas.fillRect(ROAD_X, HUD_H, ROAD_W, PORTRAIT_H - HUD_H, TFT_DARKGREY);
    canvas.drawRect(ROAD_X, HUD_H, ROAD_W, PORTRAIT_H - HUD_H, TFT_LIGHTGREY);
    for (uint8_t lane = 1; lane < LANES; lane++) {
      const int x = ROAD_X + lane * LANE_W;
      for (int y = HUD_H + 6; y < PORTRAIT_H; y += 24) {
        canvas.drawFastVLine(x, y, 13, TFT_WHITE);
      }
    }
    for (uint8_t i = 0; i < ROWS; i++) {
      if (!rows_[i].active) continue;
      for (uint8_t lane = 0; lane < LANES; lane++) {
        if (rows_[i].mask & (1 << lane)) {
          drawCar(canvas, laneX(lane), static_cast<int>(rows_[i].y), false);
        }
      }
    }
    drawCar(canvas, laneX(playerLane_), PLAYER_Y, true);
  };

  const bool buffered = TDisplayFramebuffer::draw(tft, PORTRAIT_W, PORTRAIT_H, drawScene);
  if (buffered) {
    if (!cityRacerFrameLogged) {
      cityRacerFrameLogged = true;
      Serial.print("[city-racer] shared sprite free_heap=");
      Serial.println(ESP.getFreeHeap());
    }
  } else {
    const uint32_t now = millis();
    if (now - lastSpriteFallbackLogMs > 2000) {
      lastSpriteFallbackLogMs = now;
      Serial.println("[city-racer] sprite unavailable, using tearing-prone direct draw fallback");
    }
  }
}

void CityRacerGame::drawCar(TFT_eSPI& tft, int x, int y, bool player) const {
  drawCarShape(tft, x, y, player);
}

void CityRacerGame::drawCar(TFT_eSprite& sprite, int x, int y, bool player) const {
  drawCarShape(sprite, x, y, player);
}

void CityRacerGame::drawStart(TFT_eSPI& tft) {
  loadBestScore();
  clearPortrait(tft);
  if (showStartPromptPage()) {
    drawPortraitHeader(tft, "City Racer", TFT_BLUE);
    drawCentered(tft, "Press", 87, 2, TFT_WHITE);
    drawCentered(tft, "to Start", 111, 2, TFT_LIGHTGREY);
    drawPortraitFooter(tft, "B1 left  B2 right");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    drawPortraitHeader(tft, "Top Cars", TFT_BLUE);
    String score = bestScore_ == 0 ? "--" : String(initials) + " " + String(bestScore_);
    drawCentered(tft, score, 96, 2, TFT_BLUE);
    drawPortraitFooter(tft, "Cars passed");
  } else {
    drawPortraitHeader(tft, "City Racer", TFT_BLUE);
    tft.fillRect(26, 43, 83, 164, TFT_DARKGREY);
    tft.drawRect(26, 43, 83, 164, TFT_LIGHTGREY);
    tft.drawFastVLine(53, 43, 164, TFT_WHITE);
    tft.drawFastVLine(81, 43, 164, TFT_WHITE);
    drawCar(tft, 57, 158, true);
    drawCar(tft, 31, 70, false);
    drawCar(tft, 84, 96, false);
    drawPortraitFooter(tft, "Dodge traffic");
  }
}

void CityRacerGame::drawEnd(TFT_eSPI& tft) {
  clearPortrait(tft);
  drawPortraitHeader(tft, "Crashed", TFT_RED);
  drawCentered(tft, "Cars", 64, 1, TFT_LIGHTGREY);
  drawCentered(tft, String(score_), 84, 3, TFT_YELLOW);
  String best = String(bestScore_);
  if (bestScore_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    best += " ";
    best += initials;
  }
  drawCentered(tft, "Best " + best, 139, 2, TFT_GREEN);
  drawPortraitFooter(tft, "B1 retry / hold menu");
}

int CityRacerGame::laneX(uint8_t lane) const {
  return ROAD_X + static_cast<int>(lane) * LANE_W + (LANE_W - CAR_W) / 2;
}

void CityRacerGame::loadBestScore() {
  if (bestLoaded_) return;
  cityRacerPrefs.begin("cityrace", true);
  bestScore_ = cityRacerPrefs.getUShort("best", 0);
  bestInitials_ = cityRacerPrefs.getUShort("init", PlayerProfile::defaultInitials());
  cityRacerPrefs.end();
  bestLoaded_ = true;
}

void CityRacerGame::saveBestScore() {
  bestInitials_ = PlayerProfile::loadInitials();
  cityRacerPrefs.begin("cityrace", false);
  cityRacerPrefs.putUShort("best", bestScore_);
  cityRacerPrefs.putUShort("init", bestInitials_);
  cityRacerPrefs.end();
}
