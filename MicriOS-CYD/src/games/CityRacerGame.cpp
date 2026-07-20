#include "CityRacerGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../CydHardware.h"
#include "../../PlayerProfile.h"
#include "../../CydFramebuffer.h"
namespace {
Preferences cityRacerPrefs;
bool cityRacerFrameLogged = false;
uint32_t lastSpriteFallbackLogMs = 0;

constexpr int PORTRAIT_W = 320;
constexpr int PORTRAIT_H = 208;
constexpr int HUD_H = 27;
constexpr int PLAYER_X = 38;
constexpr int ROAD_Y = HUD_H + 4;
constexpr int ROAD_H = PORTRAIT_H - ROAD_Y - 4;
constexpr int LANE_H = ROAD_H / 3;
constexpr int CAR_W = 38;
constexpr int CAR_H = 25;
constexpr uint16_t RUNNING_DRAW_MS = 33;

void setLandscape(TFT_eSPI& tft) {
  CydHardware::applyDisplayOrientation(tft);
}

void clearLandscape(TFT_eSPI& tft) {
  setLandscape(tft);
  tft.fillRect(0, 0, PORTRAIT_W, PORTRAIT_H, TFT_BLACK);
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
  const uint16_t body = player ? TFT_CYAN : TFT_RED;
  const uint16_t glass = player ? TFT_WHITE : TFT_ORANGE;
  tft.fillRoundRect(x, y + 3, CAR_W, CAR_H - 6, 4, body);
  tft.fillRect(x + 12, y + 5, 13, CAR_H - 10, glass);
  tft.drawLine(x + 30, y + 6, x + CAR_W - 2, y + 9, TFT_WHITE);
  tft.drawLine(x + 30, y + CAR_H - 7, x + CAR_W - 2, y + CAR_H - 10, TFT_WHITE);
  tft.fillRect(x + 6, y, 8, 4, TFT_BLACK);
  tft.fillRect(x + 25, y, 8, 4, TFT_BLACK);
  tft.fillRect(x + 6, y + CAR_H - 4, 8, 4, TFT_BLACK);
  tft.fillRect(x + 25, y + CAR_H - 4, 8, 4, TFT_BLACK);
  if (player) tft.fillRect(x + CAR_W - 3, y + 9, 3, 7, TFT_YELLOW);
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
    rows_[i].x -= speed_ * dt;
    if (!rows_[i].counted && rows_[i].x + CAR_W < PLAYER_X) {
      rows_[i].counted = true;
      for (uint8_t lane = 0; lane < LANES; lane++) {
        if (rows_[i].mask & (1 << lane)) score_++;
      }
      if (score_ > bestScore_) {
        bestScore_ = score_;
        saveBestScore();
      }
    }
    if (rows_[i].x < -CAR_W) rows_[i].active = false;
    if (rows_[i].x <= PLAYER_X + CAR_W &&
        rows_[i].x + CAR_W >= PLAYER_X &&
        (rows_[i].mask & (1 << playerLane_))) {
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
    if (rows_[i].active && rows_[i].x > PORTRAIT_W - 82.0f) return;
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
    rows_[i].x = PORTRAIT_W + 4.0f;
    rows_[i].mask = mask;
    return;
  }
}

void CityRacerGame::drawRunning(TFT_eSPI& tft) {
  setLandscape(tft);

  auto drawScene = [this](auto& canvas) {
    clearCanvas(canvas);
    drawPortraitHeader(canvas, "City Racer", TFT_BLUE, String("L") + String(level_) + " C" + String(score_));
    canvas.fillRect(0, ROAD_Y, PORTRAIT_W, ROAD_H, TFT_DARKGREY);
    canvas.drawRect(0, ROAD_Y, PORTRAIT_W, ROAD_H, TFT_LIGHTGREY);
    for (uint8_t lane = 1; lane < LANES; lane++) {
      const int y = ROAD_Y + lane * LANE_H;
      for (int x = 4; x < PORTRAIT_W; x += 28) {
        canvas.drawFastHLine(x, y, 16, TFT_WHITE);
      }
    }
    for (uint8_t i = 0; i < ROWS; i++) {
      if (!rows_[i].active) continue;
      for (uint8_t lane = 0; lane < LANES; lane++) {
        if (rows_[i].mask & (1 << lane)) {
          drawCar(canvas, static_cast<int>(rows_[i].x), laneY(lane), false);
        }
      }
    }
    drawCar(canvas, PLAYER_X, laneY(playerLane_), true);
  };

  const bool buffered = CydFramebuffer::draw(tft, PORTRAIT_W, PORTRAIT_H, drawScene);
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
  const bool promptPage = showStartPromptPage();
  const bool scorePage = showStartScorePage();
  CydFramebuffer::draw(tft, PORTRAIT_W, PORTRAIT_H, [&](auto& canvas) {
    clearCanvas(canvas);
    if (promptPage) {
      drawPortraitHeader(canvas, "City Racer", TFT_BLUE);
      drawCentered(canvas, "Press to Start", 76, 3, TFT_WHITE);
      drawPortraitFooter(canvas, "Tap top / bottom to change lane");
    } else if (scorePage) {
      char initials[4];
      PlayerProfile::unpackDottedInitials(bestInitials_, initials);
      drawPortraitHeader(canvas, "Top Cars", TFT_BLUE);
      drawCentered(canvas, "Cars passed", 42, 1, TFT_LIGHTGREY);
      drawCentered(canvas, bestScore_ == 0 ? String("--") : String(bestScore_), 59, 4, TFT_BLUE);
      drawCentered(canvas, bestScore_ == 0 ? String("") : String(initials), 105, 2, TFT_LIGHTGREY);
      drawPortraitFooter(canvas, "Best run");
    } else {
      drawPortraitHeader(canvas, "City Racer", TFT_BLUE);
      canvas.fillRect(0, ROAD_Y, PORTRAIT_W, ROAD_H, TFT_DARKGREY);
      for (uint8_t lane = 1; lane < LANES; lane++) {
        const int y = ROAD_Y + lane * LANE_H;
        for (int x = 4; x < PORTRAIT_W; x += 28) canvas.drawFastHLine(x, y, 16, TFT_WHITE);
      }
      drawCarShape(canvas, 46, laneY(1), true);
      drawCarShape(canvas, 180, laneY(0), false);
      drawCarShape(canvas, 242, laneY(2), false);
      drawPortraitFooter(canvas, "Dodge traffic");
    }
  });
}

void CityRacerGame::drawEnd(TFT_eSPI& tft) {
  CydFramebuffer::draw(tft, PORTRAIT_W, PORTRAIT_H, [&](auto& canvas) {
    clearCanvas(canvas);
    drawPortraitHeader(canvas, "Crashed", TFT_RED);
    drawCentered(canvas, "Cars passed", 39, 1, TFT_LIGHTGREY);
    drawCentered(canvas, String(score_), 54, 3, TFT_YELLOW);
    String best = String(bestScore_);
    if (bestScore_ > 0) {
      char initials[4];
      PlayerProfile::unpackDottedInitials(bestInitials_, initials);
      best += " ";
      best += initials;
    }
    drawCentered(canvas, "Best", 91, 1, TFT_LIGHTGREY);
    drawCentered(canvas, best, 106, 2, TFT_GREEN);
    drawPortraitFooter(canvas, "Tap Play Again");
  });
}

int CityRacerGame::laneY(uint8_t lane) const {
  return ROAD_Y + static_cast<int>(lane) * LANE_H + (LANE_H - CAR_H) / 2;
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
