#include "PipeManiaGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
constexpr uint8_t OPEN_UP = 1 << 0;
constexpr uint8_t OPEN_RIGHT = 1 << 1;
constexpr uint8_t OPEN_DOWN = 1 << 2;
constexpr uint8_t OPEN_LEFT = 1 << 3;
constexpr int GRID_X = 10;
constexpr int GRID_Y = 34;
constexpr int CELL = 18;
Preferences pipePrefs;
}

PipeManiaGame::PipeManiaGame(uint32_t width, uint32_t height)
    : App("Pipe Mania", width, height) {}

bool PipeManiaGame::hasCustomOverlay() const {
  return true;
}

void PipeManiaGame::onAppReset() {
  loadBestScore();
  level_ = 1;
  score_ = 0;
  won_ = false;
  loadLevel();
}

void PipeManiaGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)b2;
  if (b1.released) {
    placedThisHold_ = false;
  }

  if (pipeState_ == PipeState::Complete) {
    completeTimerMs_ += deltaMs;
    if (completeTimerMs_ >= 900) {
      level_++;
      loadLevel();
    }
    return;
  }

  if (pipeState_ == PipeState::Build) {
    buildTimerMs_ += deltaMs;
    if (b1.click) {
      cyclePiece();
    }
    if (b1.down && b1.holdMs > 180 && !placedThisHold_) {
      placePiece();
      placedThisHold_ = true;
    }
    if (buildTimerMs_ >= buildDelayMs()) {
      pipeState_ = PipeState::Flow;
      flowTimerMs_ = 0;
    }
  }

  if (pipeState_ == PipeState::Flow) {
    flowTimerMs_ += deltaMs;
    if (flowTimerMs_ >= flowDelayMs()) {
      flowTimerMs_ = 0;
      advanceFlow();
    }
  }
}

void PipeManiaGame::drawRunning(TFT_eSPI& tft) {
  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    const String stat = "L" + String(level_) + " " + String(flowedCount_) + "/" + String(targetCount_);
    TDisplayUi::header(canvas, "Pipe Mania", TFT_GREEN, stat.c_str());

    for (uint8_t i = 0; i < CELL_COUNT; i++) {
      const uint8_t c = i % COLS;
      const uint8_t r = i / COLS;
      const int x = GRID_X + c * CELL;
      const int y = GRID_Y + r * CELL;
      canvas.drawRect(x, y, CELL, CELL, TFT_DARKGREY);
      drawPipe(canvas, x, y, grid_[i], filled_[i]);
      if (i == cursor_ && pipeState_ == PipeState::Build) {
        if (((millis() / 180) % 2) == 0) {
          canvas.drawRect(x - 2, y - 2, CELL + 4, CELL + 4, TFT_YELLOW);
          canvas.drawRect(x - 3, y - 3, CELL + 6, CELL + 6, TFT_ORANGE);
        }
        drawPipe(canvas, x, y, currentPiece_, false);
      }
    }

    canvas.setTextSize(2);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("Next", 158, 38);
    canvas.drawRect(168, 62, CELL + 8, CELL + 8, TFT_DARKGREY);
    drawPipe(canvas, 172, 66, currentPiece_, false);
    if (pipeState_ == PipeState::Build) {
      if (cursor_ >= CELL_COUNT) {
        TDisplayUi::centered(canvas, "Blocked", 101, 2, TFT_RED);
      } else {
        TDisplayUi::largeValue(canvas, String((buildDelayMs() - min(buildTimerMs_, buildDelayMs())) / 1000), 91, TFT_YELLOW);
      }
    } else if (pipeState_ == PipeState::Complete) {
      TDisplayUi::centered(canvas, "Clear", 96, 2, TFT_GREEN);
    } else {
      TDisplayUi::centered(canvas, "Goo", 96, 2, TFT_GREEN);
    }
    TDisplayUi::footer(canvas, pipeState_ == PipeState::Build ? "B1 piece  Hold place" : "Flowing...");
  });
}

void PipeManiaGame::drawStart(TFT_eSPI& tft) {
  TDisplayUi::clear(tft);
  loadBestScore();
  if (showStartPromptPage()) {
    TDisplayUi::header(tft, appTitle(), TFT_GREEN);
    TDisplayUi::centered(tft, "Press", 50, 3, TFT_WHITE);
    TDisplayUi::centered(tft, "to Start", 78, 2, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "B1 start");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);

    TDisplayUi::header(tft, "Top Score", TFT_YELLOW);
    TDisplayUi::largeValue(tft, bestScore_ == 0 ? String("--") : String(bestScore_), 54, TFT_YELLOW);
    TDisplayUi::centered(tft, bestScore_ == 0 ? String("") : String(initials), 96, 2, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "Best pipe score");
  } else {
    TDisplayUi::header(tft, appTitle(), TFT_GREEN);
    drawPipe(tft, 58, 56, Horizontal, true);
    drawPipe(tft, 76, 56, TurnRD, true);
    drawPipe(tft, 76, 74, Vertical, true);
    drawPipe(tft, 76, 92, TurnUR, true);
    drawPipe(tft, 94, 92, Horizontal, true);
    TDisplayUi::centered(tft, "Outrun the goo", 88, 2, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "B1 piece  Hold place");
  }
}

void PipeManiaGame::drawEnd(TFT_eSPI& tft) {
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, won_ ? "Pipe Master" : "Pipe End", won_ ? TFT_GREEN : TFT_RED);
  TDisplayUi::labelValue(tft, 48, "Score", String(score_), TFT_GREEN);
  String best = String(bestScore_);
  if (bestScore_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    best += " ";
    best += initials;
  }
  TDisplayUi::labelValue(tft, 78, "Best", best, TFT_YELLOW);
  TDisplayUi::footer(tft, "B1 retry  Hold menu");
}

void PipeManiaGame::loadBestScore() {
  if (bestLoaded_) {
    return;
  }
  pipePrefs.begin("pipe", true);
  bestScore_ = pipePrefs.getUShort("best", 0);
  bestInitials_ = pipePrefs.getUShort("init", PlayerProfile::defaultInitials());
  pipePrefs.end();
  bestLoaded_ = true;
}

void PipeManiaGame::saveBestScore() {
  bestInitials_ = PlayerProfile::loadInitials();
  pipePrefs.begin("pipe", false);
  pipePrefs.putUShort("best", bestScore_);
  pipePrefs.putUShort("init", bestInitials_);
  pipePrefs.end();
}

void PipeManiaGame::loadLevel() {
  for (uint8_t i = 0; i < CELL_COUNT; i++) {
    grid_[i] = Empty;
    filled_[i] = false;
  }
  pipeState_ = PipeState::Build;
  currentPiece_ = static_cast<PipeType>(random(Horizontal, TurnLU + 1));
  flowCell_ = ROWS / 2 * COLS;
  buildEndCell_ = flowCell_;
  buildDir_ = Right;
  flowDir_ = Right;
  flowedCount_ = 1;
  targetCount_ = min<uint8_t>(20, 6 + level_);
  buildTimerMs_ = 0;
  cursorTimerMs_ = 0;
  flowTimerMs_ = 0;
  completeTimerMs_ = 0;
  placedThisHold_ = false;
  grid_[flowCell_] = Horizontal;
  filled_[flowCell_] = true;
  updateCursorToPipeEnd();
}

uint16_t PipeManiaGame::buildDelayMs() const {
  return max<uint16_t>(9000, 22000 - level_ * 650);
}

uint16_t PipeManiaGame::flowDelayMs() const {
  return max<uint16_t>(650, 1800 - level_ * 70);
}

void PipeManiaGame::cyclePiece() {
  currentPiece_ = static_cast<PipeType>(currentPiece_ + 1);
  if (currentPiece_ > TurnLU) {
    currentPiece_ = Horizontal;
  }
}

void PipeManiaGame::placePiece() {
  if (cursor_ >= CELL_COUNT || cursor_ == flowCell_ || filled_[cursor_] || grid_[cursor_] != Empty) {
    return;
  }
  const Direction enteredFrom = opposite(buildDir_);
  if (!hasOpening(currentPiece_, enteredFrom)) {
    return;
  }
  grid_[cursor_] = currentPiece_;
  buildEndCell_ = cursor_;
  buildDir_ = nextDirection(currentPiece_, enteredFrom);
  updateCursorToPipeEnd();
  currentPiece_ = static_cast<PipeType>(random(Horizontal, TurnLU + 1));
}

void PipeManiaGame::updateCursorToPipeEnd() {
  uint8_t next = CELL_COUNT;
  if (!stepCell(buildEndCell_, buildDir_, next) || grid_[next] != Empty || filled_[next]) {
    cursor_ = CELL_COUNT;
    return;
  }
  cursor_ = next;
}

void PipeManiaGame::advanceFlow() {
  if (!hasOpening(grid_[flowCell_], flowDir_)) {
    finishLevel();
    return;
  }

  uint8_t next = 0;
  if (!stepCell(flowCell_, flowDir_, next)) {
    finishLevel();
    return;
  }

  const Direction enteredFrom = opposite(flowDir_);
  if (grid_[next] == Empty || !hasOpening(grid_[next], enteredFrom)) {
    finishLevel();
    return;
  }

  flowCell_ = next;
  filled_[flowCell_] = true;
  flowedCount_++;
  score_ += 5;
  flowDir_ = nextDirection(grid_[flowCell_], enteredFrom);
}

void PipeManiaGame::finishLevel() {
  if (flowedCount_ >= targetCount_) {
    pipeState_ = PipeState::Complete;
    completeTimerMs_ = 0;
    score_ += flowedCount_;
    if (score_ > bestScore_) {
      bestScore_ = score_;
      saveBestScore();
    }
  } else {
    won_ = false;
    if (score_ > bestScore_) {
      bestScore_ = score_;
      saveBestScore();
    }
    endApp();
  }
}

uint8_t PipeManiaGame::openings(PipeType pipe) const {
  switch (pipe) {
    case Horizontal:
      return OPEN_LEFT | OPEN_RIGHT;
    case Vertical:
      return OPEN_UP | OPEN_DOWN;
    case TurnUR:
      return OPEN_UP | OPEN_RIGHT;
    case TurnRD:
      return OPEN_RIGHT | OPEN_DOWN;
    case TurnDL:
      return OPEN_DOWN | OPEN_LEFT;
    case TurnLU:
      return OPEN_LEFT | OPEN_UP;
    case Empty:
    default:
      return 0;
  }
}

bool PipeManiaGame::hasOpening(PipeType pipe, Direction dir) const {
  return (openings(pipe) & (1 << dir)) != 0;
}

PipeManiaGame::Direction PipeManiaGame::opposite(Direction dir) const {
  return static_cast<Direction>((dir + 2) % 4);
}

PipeManiaGame::Direction PipeManiaGame::nextDirection(PipeType pipe, Direction enteredFrom) const {
  const uint8_t open = openings(pipe) & ~(1 << enteredFrom);
  for (uint8_t d = 0; d < 4; d++) {
    if (open & (1 << d)) {
      return static_cast<Direction>(d);
    }
  }
  return enteredFrom;
}

bool PipeManiaGame::stepCell(uint8_t cell, Direction dir, uint8_t& nextCell) const {
  const uint8_t c = cell % COLS;
  const uint8_t r = cell / COLS;
  if (dir == Up && r > 0) {
    nextCell = cell - COLS;
    return true;
  }
  if (dir == Right && c < (COLS - 1)) {
    nextCell = cell + 1;
    return true;
  }
  if (dir == Down && r < (ROWS - 1)) {
    nextCell = cell + COLS;
    return true;
  }
  if (dir == Left && c > 0) {
    nextCell = cell - 1;
    return true;
  }
  return false;
}

template <typename Canvas>
void PipeManiaGame::drawPipe(Canvas& tft, int x, int y, PipeType pipe, bool filled) {
  const int cx = x + CELL / 2;
  const int cy = y + CELL / 2;
  const int r = max(2, CELL / 6);
  if (pipe == Empty) {
    return;
  }
  if (filled) {
    tft.fillRect(x + 2, y + 2, CELL - 4, CELL - 4, TFT_DARKGREEN);
  }
  const uint16_t pipeColor = filled ? TFT_GREEN : TFT_LIGHTGREY;
  const uint16_t coreColor = filled ? TFT_YELLOW : TFT_DARKGREY;
  if (hasOpening(pipe, Up)) {
    tft.fillRect(cx - r, y, r * 2 + 1, CELL / 2 + 1, pipeColor);
  }
  if (hasOpening(pipe, Right)) {
    tft.fillRect(cx, cy - r, CELL / 2, r * 2 + 1, pipeColor);
  }
  if (hasOpening(pipe, Down)) {
    tft.fillRect(cx - r, cy, r * 2 + 1, CELL / 2, pipeColor);
  }
  if (hasOpening(pipe, Left)) {
    tft.fillRect(x, cy - r, CELL / 2 + 1, r * 2 + 1, pipeColor);
  }
  tft.fillCircle(cx, cy, r + 1, coreColor);
  if (filled) tft.fillCircle(cx, cy, r - 1, TFT_GREEN);
}
