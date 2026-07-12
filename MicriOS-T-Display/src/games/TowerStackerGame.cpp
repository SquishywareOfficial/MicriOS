#include "TowerStackerGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
Preferences towerScorePrefs;
}

TowerStackerGame::TowerStackerGame(uint32_t width, uint32_t height)
    : App("Tower Stacker", width, height) {}

bool TowerStackerGame::hasCustomOverlay() const {
  return true;
}

void TowerStackerGame::onAppReset() {
  loadHighScore();
  score_ = 0;
  level_ = 1;
  startTowerLevel();
}

void TowerStackerGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)b2;
  const float deltaSec = static_cast<float>(deltaMs) * 0.001f;
  movingX_ += static_cast<float>(movingDir_) * movingSpeed_ * deltaSec;

  const float minX = 1.0f;
  const float maxX = static_cast<float>(width - movingW_ - 1);
  if (movingX_ <= minX) {
    movingX_ = minX;
    movingDir_ = 1;
  } else if (movingX_ >= maxX) {
    movingX_ = maxX;
    movingDir_ = -1;
  }

  if (b1.click) {
    dropMovingBlock();
  }
}

void TowerStackerGame::drawRunning(TFT_eSPI& tft) {
  TDisplayFramebuffer::draw(tft, static_cast<int16_t>(width), static_cast<int16_t>(height), [&](auto& canvas) {
    TDisplayUi::clear(canvas);
    TDisplayUi::header(canvas, "Tower Stacker", TFT_CYAN, (String("L") + String(level_) + " S" + String(score_)).c_str());
    canvas.drawFastHLine(0, height - 1, width, TFT_DARKGREY);

    const uint8_t visibleLayers = (height - HUD_H - 3) / BLOCK_H;
    uint8_t firstVisibleLayer = 0;
    if (layerCount_ >= visibleLayers) {
      firstVisibleLayer = layerCount_ - visibleLayers + 1;
    }

    for (uint8_t i = firstVisibleLayer; i < layerCount_; i++) {
      canvas.fillRect(layers_[i].x, layerY(i, firstVisibleLayer), layers_[i].w, BLOCK_H - 1,
                      i % 2 == 0 ? TFT_CYAN : TFT_BLUE);
    }

    if (layerCount_ < MAX_LAYERS) {
      canvas.fillRect(static_cast<int>(movingX_), layerY(layerCount_, firstVisibleLayer), movingW_, BLOCK_H - 1, TFT_YELLOW);
      canvas.drawRect(static_cast<int>(movingX_), layerY(layerCount_, firstVisibleLayer), movingW_, BLOCK_H - 1, TFT_WHITE);
    }
  });
}

void TowerStackerGame::drawStart(TFT_eSPI& tft) { tft.fillScreen(TFT_BLACK);
  loadHighScore();
  TDisplayUi::clear(tft);
  if (showStartPromptPage()) {
    TDisplayUi::header(tft, "Tower Stacker", TFT_CYAN);
    TDisplayUi::centered(tft, "Press to Start", 56, 2, TFT_WHITE);
    TDisplayUi::footer(tft, "B1 drops the moving block");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(highScoreInitials_, initials);
    TDisplayUi::header(tft, "Top Score", TFT_CYAN);
    String score = highScore_ == 0 ? "--" : String(initials) + " " + String(highScore_);
    TDisplayUi::largeValue(tft, score, 54, TFT_CYAN);
    TDisplayUi::footer(tft, "Total stacked blocks");
  } else {
    TDisplayUi::header(tft, "Tower Stacker", TFT_CYAN);
    tft.fillRect(106, 44, 28, 56, TFT_BLUE);
    tft.fillRect(92, 62, 56, 38, TFT_CYAN);
    tft.fillRect(75, 81, 90, 19, TFT_YELLOW);
    tft.drawFastHLine(58, 101, 124, TFT_LIGHTGREY);
    TDisplayUi::centered(tft, "Stack clean", 112, 1, TFT_LIGHTGREY);
  }
}

void TowerStackerGame::drawEnd(TFT_eSPI& tft) { tft.fillScreen(TFT_BLACK);
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Game Over", TFT_RED);
  TDisplayUi::labelValue(tft, 49, "Score", String(score_), TFT_YELLOW);
  String best = String(highScore_);
  if (highScore_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(highScoreInitials_, initials);
    best += " ";
    best += initials;
  }
  TDisplayUi::labelValue(tft, 78, "Best", best, TFT_GREEN);
  TDisplayUi::footer(tft, "B1 retry / B1 hold menu");
}

void TowerStackerGame::loadHighScore() {
  if (highScoreLoaded_) {
    return;
  }
  towerScorePrefs.begin("tower", true);
  highScore_ = towerScorePrefs.getUShort("best", 0);
  highScoreInitials_ = towerScorePrefs.getUShort("init", PlayerProfile::defaultInitials());
  towerScorePrefs.end();
  highScoreLoaded_ = true;
}

void TowerStackerGame::saveHighScore() {
  highScoreInitials_ = PlayerProfile::loadInitials();
  towerScorePrefs.begin("tower", false);
  towerScorePrefs.putUShort("best", highScore_);
  towerScorePrefs.putUShort("init", highScoreInitials_);
  towerScorePrefs.end();
}

void TowerStackerGame::startTowerLevel() {
  const int baseW = 112;
  layerCount_ = 1;
  towerHeight_ = 0;
  layers_[0].x = (static_cast<int>(width) - baseW) / 2;
  layers_[0].w = baseW;
  movingDir_ = 1;
  movingSpeed_ = 72.0f + static_cast<float>(level_ - 1) * 9.0f;
  prepareNextBlock(baseW);
}

void TowerStackerGame::dropMovingBlock() {
  const Layer& previous = layers_[layerCount_ - 1];
  const int droppedX = static_cast<int>(movingX_ + 0.5f);
  const int leftEdge = max(droppedX, previous.x);
  const int rightEdge = min(droppedX + movingW_, previous.x + previous.w);
  const int overlapW = rightEdge - leftEdge;

  if (overlapW <= 0) {
    endApp();
    return;
  }

  if (layerCount_ >= MAX_LAYERS) {
    level_++;
    startTowerLevel();
    return;
  }

  int nextX = leftEdge;
  int nextW = overlapW;
  const int miss = previous.w - overlapW;
  if (miss <= 2) {
    nextX = previous.x;
    nextW = previous.w;
  }
  layers_[layerCount_].x = nextX;
  layers_[layerCount_].w = nextW;
  layerCount_++;
  towerHeight_++;
  score_++;
  if (score_ > highScore_) {
    highScore_ = score_;
    saveHighScore();
  }

  if (towerHeight_ >= LEVEL_HEIGHT) {
    level_++;
    startTowerLevel();
  } else {
    prepareNextBlock(nextW);
  }
}

void TowerStackerGame::prepareNextBlock(int blockWidth) {
  movingW_ = blockWidth;
  movingX_ = 1.0f;
  movingDir_ = 1;
  movingSpeed_ = min(145.0f, 72.0f + static_cast<float>(level_ - 1) * 9.0f + static_cast<float>(towerHeight_) * 2.5f);
}

int TowerStackerGame::layerY(uint8_t layer, uint8_t firstVisibleLayer) const {
  const uint8_t visibleIndex = layer - firstVisibleLayer;
  return static_cast<int>(height) - 2 - ((visibleIndex + 1) * BLOCK_H);
}
