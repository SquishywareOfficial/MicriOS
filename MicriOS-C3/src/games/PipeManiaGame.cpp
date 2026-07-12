#include "PipeManiaGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
constexpr uint8_t OPEN_UP = 1 << 0;
constexpr uint8_t OPEN_RIGHT = 1 << 1;
constexpr uint8_t OPEN_DOWN = 1 << 2;
constexpr uint8_t OPEN_LEFT = 1 << 3;
constexpr int GRID_X = 2;
constexpr int GRID_Y = 8;
constexpr int CELL = 6;
Preferences pipePrefs;
}

PipeManiaGame::PipeManiaGame(uint32_t width, uint32_t height, uint32_t left)
    : App("Pipe Mania", width, height), left_(left) {}

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

void PipeManiaGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
  if (input.released) {
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
    if (input.click) {
      cyclePiece();
    }
    if (input.down && input.holdMs > 180 && !placedThisHold_) {
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

void PipeManiaGame::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 2, 6);
  u8g2.print("L");
  u8g2.print(level_);
  u8g2.print(" ");
  u8g2.print(flowedCount_);
  u8g2.print("/");
  u8g2.print(targetCount_);

  for (uint8_t i = 0; i < CELL_COUNT; i++) {
    const uint8_t c = i % COLS;
    const uint8_t r = i / COLS;
    const int x = left_ + GRID_X + c * CELL;
    const int y = GRID_Y + r * CELL;
    u8g2.drawFrame(x, y, CELL, CELL);
    drawPipe(u8g2, x, y, grid_[i], filled_[i]);
    if (i == cursor_ && pipeState_ == PipeState::Build) {
      if (((millis() / 180) % 2) == 0) {
        u8g2.drawFrame(x - 2, y - 2, CELL + 4, CELL + 4);
      }
      drawPipe(u8g2, x, y, currentPiece_, false);
    }
  }

  u8g2.drawStr(left_ + 48, 13, "Next");
  drawPipe(u8g2, left_ + 54, 17, currentPiece_, false);
  if (pipeState_ == PipeState::Build) {
    u8g2.setCursor(left_ + 43, 36);
    if (cursor_ >= CELL_COUNT) {
      u8g2.print("Blocked");
    } else {
      u8g2.print("T");
      u8g2.print((buildDelayMs() - min(buildTimerMs_, buildDelayMs())) / 1000);
    }
  } else if (pipeState_ == PipeState::Complete) {
    u8g2.drawStr(left_ + 49, 36, "Clear");
  } else {
    u8g2.drawStr(left_ + 50, 36, "Goo");
  }
}

void PipeManiaGame::drawStart(U8G2& u8g2) {
  loadBestScore();
  u8g2.drawFrame(0, 0, width + 2, height);
  if (showStartPromptPage()) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(20, 16, "Press");
    u8g2.drawStr(13, 29, "to Start");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 9, "Top Score");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(3, 24);
    if (bestScore_ == 0) {
      u8g2.print("--");
    } else {
      u8g2.print(initials);
      u8g2.print(" ");
      u8g2.print(bestScore_);
    }
  } else {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 9, appTitle());
    drawPipe(u8g2, 8, 16, Horizontal, true);
    drawPipe(u8g2, 14, 16, TurnRD, true);
    drawPipe(u8g2, 14, 22, Vertical, true);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(30, 20, "Tap piece");
    u8g2.drawStr(30, 29, "Hold place");
  }
}

void PipeManiaGame::drawEnd(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, won_ ? "Pipe Master" : "Pipe End");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(3, 20);
  u8g2.print("Score ");
  u8g2.print(score_);
  u8g2.setCursor(3, 29);
  u8g2.print("Best ");
  u8g2.print(bestScore_);
  if (bestScore_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.print(" ");
    u8g2.print(initials);
  }
  u8g2.drawStr(3, 38, "Tap retry Hold menu");
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

void PipeManiaGame::drawPipe(U8G2& u8g2, int x, int y, PipeType pipe, bool filled) {
  const int cx = x + 3;
  const int cy = y + 3;
  if (pipe == Empty) {
    return;
  }
  if (filled) {
    u8g2.drawBox(x + 1, y + 1, CELL - 2, CELL - 2);
    u8g2.setDrawColor(0);
  }
  if (hasOpening(pipe, Up)) {
    u8g2.drawLine(cx, cy, cx, y);
  }
  if (hasOpening(pipe, Right)) {
    u8g2.drawLine(cx, cy, x + CELL - 1, cy);
  }
  if (hasOpening(pipe, Down)) {
    u8g2.drawLine(cx, cy, cx, y + CELL - 1);
  }
  if (hasOpening(pipe, Left)) {
    u8g2.drawLine(cx, cy, x, cy);
  }
  if (filled) {
    u8g2.drawDisc(cx, cy, 1);
    u8g2.setDrawColor(1);
  }
}
