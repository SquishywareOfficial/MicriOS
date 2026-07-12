#include "MazeRunnerGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../../PlayerProfile.h"
#include "../../TDisplayFramebuffer.h"
#include "../../TDisplayUi.h"

namespace {
constexpr uint8_t WALL_UP = 1 << 0;
constexpr uint8_t WALL_RIGHT = 1 << 1;
constexpr uint8_t WALL_DOWN = 1 << 2;
constexpr uint8_t WALL_LEFT = 1 << 3;
constexpr int GRID_X = 21;
constexpr int GRID_Y = 36;
constexpr int CELL_W = 22;
constexpr int CELL_H = 15;
Preferences mazePrefs;
}

MazeRunnerGame::MazeRunnerGame(uint32_t width, uint32_t height, uint32_t left, bool collectorMode)
    : App(collectorMode ? "Maze Collector" : "Maze Runner", width, height), collectorMode_(collectorMode) {
  (void)left;
}

bool MazeRunnerGame::hasCustomOverlay() const {
  return true;
}

void MazeRunnerGame::onAppReset() {
  loadBestLevel();
  level_ = 1;
  nextLevel();
}

void MazeRunnerGame::updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) {
  (void)b2;
  if (timeLeftMs_ > deltaMs) {
    timeLeftMs_ -= deltaMs;
  } else {
    timeLeftMs_ = 0;
    endApp();
    return;
  }

  if (runnerState_ == RunnerState::Choosing) {
    if (optionCount_ == 0) {
      collectOptions();
      if (optionCount_ == 0) {
        endApp();
        return;
      }
    }
    if (b1.click && optionCount_ > 0) {
      selectedOption_ = (selectedOption_ + 1) % optionCount_;
    }
    if ((b1.down && b1.holdMs > 300) || b1.longPress) {
      chooseDirection();
    }
    return;
  }

  stepTimerMs_ += deltaMs;
  const uint16_t stepDelay = max<uint16_t>(180, 430 - (level_ * 18));
  if (stepTimerMs_ >= stepDelay) {
    stepTimerMs_ = 0;
    stepRunner();
  }
}

void MazeRunnerGame::drawRunning(TFT_eSPI& tft) {
  TDisplayFramebuffer::draw(tft, width, height, [&](auto& canvas) {
    String stat = "L" + String(level_) + " T" + String(timeLeftMs_ / 1000);
    if (collectorMode_) {
      stat += " K";
      stat += String(pickupCount_ - __builtin_popcount(collectedMask_));
    }
    TDisplayUi::header(canvas, collectorMode_ ? "Maze Collector" : "Maze Runner", collectorMode_ ? TFT_YELLOW : TFT_CYAN, stat.c_str());

    for (uint8_t r = 0; r < ROWS; r++) {
      for (uint8_t c = 0; c < COLS; c++) {
        const uint8_t cell = r * COLS + c;
        const int x = GRID_X + c * CELL_W;
        const int y = GRID_Y + r * CELL_H;
        if (walls_[cell] & WALL_UP) {
          canvas.drawLine(x, y, x + CELL_W, y, TFT_LIGHTGREY);
          canvas.drawLine(x, y + 1, x + CELL_W, y + 1, TFT_DARKGREY);
        }
        if (walls_[cell] & WALL_LEFT) {
          canvas.drawLine(x, y, x, y + CELL_H, TFT_LIGHTGREY);
          canvas.drawLine(x + 1, y, x + 1, y + CELL_H, TFT_DARKGREY);
        }
        if (walls_[cell] & WALL_RIGHT) {
          canvas.drawLine(x + CELL_W, y, x + CELL_W, y + CELL_H, TFT_LIGHTGREY);
          canvas.drawLine(x + CELL_W - 1, y, x + CELL_W - 1, y + CELL_H, TFT_DARKGREY);
        }
        if (walls_[cell] & WALL_DOWN) {
          canvas.drawLine(x, y + CELL_H, x + CELL_W, y + CELL_H, TFT_LIGHTGREY);
          canvas.drawLine(x, y + CELL_H - 1, x + CELL_W, y + CELL_H - 1, TFT_DARKGREY);
        }
      }
    }

    const uint8_t runnerCol = runnerCell_ % COLS;
    const uint8_t runnerRow = runnerCell_ / COLS;
    const int runnerX = GRID_X + runnerCol * CELL_W + CELL_W / 2;
    const int runnerY = GRID_Y + runnerRow * CELL_H + CELL_H / 2;
    if (collectorMode_) {
      for (uint8_t i = 0; i < pickupCount_; i++) {
        if ((collectedMask_ & (1 << i)) != 0) {
          continue;
        }
        const uint8_t pickupCol = pickupCells_[i] % COLS;
        const uint8_t pickupRow = pickupCells_[i] / COLS;
        const int pickupX = GRID_X + pickupCol * CELL_W + CELL_W / 2;
        const int pickupY = GRID_Y + pickupRow * CELL_H + CELL_H / 2;
        canvas.fillCircle(pickupX, pickupY, 4, TFT_YELLOW);
        canvas.drawCircle(pickupX, pickupY, 5, TFT_ORANGE);
      }
    }
    canvas.fillCircle(runnerX, runnerY, 5, TFT_GREEN);
    canvas.drawPixel(runnerX + 2, runnerY - 1, TFT_BLACK);
    if (runnerState_ == RunnerState::Choosing && optionCount_ > 0) {
      const Direction option = options_[selectedOption_];
      int x2 = runnerX;
      int y2 = runnerY;
      if (option == Up) {
        y2 -= 11;
      } else if (option == Right) {
        x2 += 13;
      } else if (option == Down) {
        y2 += 11;
      } else {
        x2 -= 13;
      }
      canvas.drawLine(runnerX, runnerY, x2, y2, TFT_CYAN);
      canvas.fillCircle(x2, y2, 2, TFT_CYAN);
    }
    const int exitX = GRID_X + (COLS - 1) * CELL_W + CELL_W / 2 - 5;
    const int exitY = GRID_Y + (ROWS - 1) * CELL_H + CELL_H / 2 - 5;
    canvas.drawRoundRect(exitX, exitY, 10, 10, 2, exitUnlocked() ? TFT_GREEN : TFT_RED);
    if (collectorMode_ && !exitUnlocked()) {
      canvas.drawLine(exitX + 2, exitY + 2, exitX + 8, exitY + 8, TFT_RED);
      canvas.drawLine(exitX + 8, exitY + 2, exitX + 2, exitY + 8, TFT_RED);
    }
    TDisplayUi::footer(canvas, runnerState_ == RunnerState::Choosing ? "B1 choose  Hold run" : "Running...");
  });
}

void MazeRunnerGame::drawStart(TFT_eSPI& tft) {
  TDisplayUi::clear(tft);
  loadBestLevel();
  if (showStartPromptPage()) {
    TDisplayUi::header(tft, appTitle(), collectorMode_ ? TFT_YELLOW : TFT_CYAN);
    TDisplayUi::centered(tft, "Press", 50, 3, TFT_WHITE);
    TDisplayUi::centered(tft, "to Start", 78, 2, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "B1 start");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);

    TDisplayUi::header(tft, "Top Level", TFT_YELLOW);
    TDisplayUi::largeValue(tft, bestLevel_ == 0 ? String("--") : "L" + String(bestLevel_), 54, TFT_YELLOW);
    TDisplayUi::centered(tft, bestLevel_ == 0 ? String("") : String(initials), 96, 2, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "Best maze level");
  } else {
    TDisplayUi::header(tft, appTitle(), collectorMode_ ? TFT_YELLOW : TFT_CYAN);
    for (uint8_t x = 0; x < 5; x++) {
      tft.drawRect(62 + x * 22, 50, 22, 18, TFT_DARKGREY);
      if (x != 2) tft.drawFastVLine(62 + x * 22, 50, 18, TFT_LIGHTGREY);
    }
    tft.fillCircle(99, 59, 5, TFT_GREEN);
    if (collectorMode_) tft.fillCircle(144, 59, 4, TFT_YELLOW);
    TDisplayUi::centered(tft, collectorMode_ ? "Collect keys" : "Choose paths", 85, 2, TFT_LIGHTGREY);
    TDisplayUi::footer(tft, "B1 choose  Hold run");
  }
}

void MazeRunnerGame::drawEnd(TFT_eSPI& tft) {
  TDisplayUi::clear(tft);
  TDisplayUi::header(tft, "Lost", TFT_RED);
  TDisplayUi::labelValue(tft, 48, "Level", String(level_), TFT_CYAN);
  String best = "L" + String(bestLevel_);
  if (bestLevel_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    best += " ";
    best += initials;
  }
  TDisplayUi::labelValue(tft, 78, "Best", best, TFT_YELLOW);
  TDisplayUi::footer(tft, "B1 retry  Hold menu");
}

void MazeRunnerGame::loadBestLevel() {
  if (bestLoaded_) {
    return;
  }
  mazePrefs.begin(collectorMode_ ? "mazecol" : "maze", true);
  bestLevel_ = mazePrefs.getUChar("best", 0);
  bestInitials_ = mazePrefs.getUShort("init", PlayerProfile::defaultInitials());
  mazePrefs.end();
  bestLoaded_ = true;
}

void MazeRunnerGame::saveBestLevel() {
  bestInitials_ = PlayerProfile::loadInitials();
  mazePrefs.begin(collectorMode_ ? "mazecol" : "maze", false);
  mazePrefs.putUChar("best", bestLevel_);
  mazePrefs.putUShort("init", bestInitials_);
  mazePrefs.end();
}

void MazeRunnerGame::generateMaze() {
  for (uint8_t i = 0; i < CELL_COUNT; i++) {
    walls_[i] = WALL_UP | WALL_RIGHT | WALL_DOWN | WALL_LEFT;
  }

  bool visited[CELL_COUNT] = {};
  uint8_t stack[CELL_COUNT] = {};
  uint8_t stackSize = 1;
  stack[0] = 0;
  visited[0] = true;

  while (stackSize > 0) {
    const uint8_t cell = stack[stackSize - 1];
    Direction options[4];
    uint8_t optionCount = 0;
    for (uint8_t d = 0; d < 4; d++) {
      const Direction dir = static_cast<Direction>(d);
      const uint8_t n = neighborCell(cell, dir);
      if (n < CELL_COUNT && !visited[n]) {
        options[optionCount++] = dir;
      }
    }
    if (optionCount == 0) {
      stackSize--;
      continue;
    }
    const Direction dir = options[random(0, optionCount)];
    const uint8_t next = neighborCell(cell, dir);
    removeWall(cell, dir);
    visited[next] = true;
    stack[stackSize++] = next;
  }
}

void MazeRunnerGame::placePickups() {
  pickupCount_ = collectorMode_ ? min<uint8_t>(3, 1 + (level_ >= 3 ? 1 : 0) + (level_ >= 6 ? 1 : 0)) : 0;
  collectedMask_ = 0;
  for (uint8_t i = 0; i < pickupCount_; i++) {
    uint8_t cell = 0;
    bool unique = false;
    for (uint8_t tries = 0; tries < 40 && !unique; tries++) {
      cell = static_cast<uint8_t>(random(1, CELL_COUNT - 1));
      unique = true;
      for (uint8_t previous = 0; previous < i; previous++) {
        if (pickupCells_[previous] == cell) {
          unique = false;
          break;
        }
      }
    }
    pickupCells_[i] = cell;
  }
}

void MazeRunnerGame::collectPickupAtRunner() {
  if (!collectorMode_) {
    return;
  }
  for (uint8_t i = 0; i < pickupCount_; i++) {
    if (pickupCells_[i] == runnerCell_) {
      collectedMask_ |= (1 << i);
    }
  }
}

void MazeRunnerGame::nextLevel() {
  generateMaze();
  placePickups();
  runnerCell_ = 0;
  dir_ = Right;
  runnerState_ = RunnerState::Choosing;
  stepTimerMs_ = 0;
  timeLeftMs_ = collectorMode_
                    ? max<uint16_t>(18000, 34000 - level_ * 750)
                    : max<uint16_t>(15000, 28000 - level_ * 750);
  turnRequested_ = false;
  collectOptions();
  if (level_ > bestLevel_) {
    bestLevel_ = level_;
    saveBestLevel();
  }
}

void MazeRunnerGame::collectOptions() {
  optionCount_ = 0;
  const Direction back = reverse(dir_);
  for (uint8_t d = 0; d < 4; d++) {
    const Direction candidate = static_cast<Direction>(d);
    if (candidate == back) {
      continue;
    }
    if (canMove(runnerCell_, candidate)) {
      options_[optionCount_++] = candidate;
    }
  }

  if (optionCount_ == 0 && canMove(runnerCell_, back)) {
    options_[optionCount_++] = back;
  }
  selectedOption_ = 0;
}

void MazeRunnerGame::chooseDirection() {
  if (optionCount_ == 0) {
    collectOptions();
  }
  if (optionCount_ == 0) {
    endApp();
    return;
  }
  dir_ = options_[selectedOption_];
  runnerState_ = RunnerState::Moving;
  stepTimerMs_ = 0;
}

void MazeRunnerGame::stepRunner() {
  if (canMove(runnerCell_, dir_)) {
    runnerCell_ = neighborCell(runnerCell_, dir_);
    collectPickupAtRunner();
  } else {
    runnerState_ = RunnerState::Choosing;
    collectOptions();
    return;
  }

  if (runnerCell_ == (CELL_COUNT - 1) && exitUnlocked()) {
    level_++;
    nextLevel();
    return;
  }

  if (shouldStopForChoice()) {
    runnerState_ = RunnerState::Choosing;
    collectOptions();
  } else {
    collectOptions();
    if (optionCount_ == 1) {
      dir_ = options_[0];
    }
  }
}

bool MazeRunnerGame::exitUnlocked() const {
  if (!collectorMode_) {
    return true;
  }
  return collectedMask_ == ((1 << pickupCount_) - 1);
}

bool MazeRunnerGame::shouldStopForChoice() const {
  uint8_t count = 0;
  const Direction back = reverse(dir_);
  for (uint8_t d = 0; d < 4; d++) {
    const Direction candidate = static_cast<Direction>(d);
    if (candidate == back) {
      continue;
    }
    if (canMove(runnerCell_, candidate)) {
      count++;
    }
  }
  return count != 1;
}

bool MazeRunnerGame::canMove(uint8_t cell, Direction dir) const {
  if (cell >= CELL_COUNT) {
    return false;
  }
  switch (dir) {
    case Up:
      return (walls_[cell] & WALL_UP) == 0;
    case Right:
      return (walls_[cell] & WALL_RIGHT) == 0;
    case Down:
      return (walls_[cell] & WALL_DOWN) == 0;
    case Left:
      return (walls_[cell] & WALL_LEFT) == 0;
  }
  return false;
}

MazeRunnerGame::Direction MazeRunnerGame::turnRight(Direction dir) const {
  return static_cast<Direction>((dir + 1) % 4);
}

MazeRunnerGame::Direction MazeRunnerGame::turnLeft(Direction dir) const {
  return static_cast<Direction>((dir + 3) % 4);
}

MazeRunnerGame::Direction MazeRunnerGame::reverse(Direction dir) const {
  return static_cast<Direction>((dir + 2) % 4);
}

uint8_t MazeRunnerGame::neighborCell(uint8_t cell, Direction dir) const {
  const uint8_t col = cell % COLS;
  const uint8_t row = cell / COLS;
  if (dir == Up && row > 0) {
    return cell - COLS;
  }
  if (dir == Right && col < (COLS - 1)) {
    return cell + 1;
  }
  if (dir == Down && row < (ROWS - 1)) {
    return cell + COLS;
  }
  if (dir == Left && col > 0) {
    return cell - 1;
  }
  return CELL_COUNT;
}

void MazeRunnerGame::removeWall(uint8_t a, Direction dir) {
  const uint8_t b = neighborCell(a, dir);
  if (b >= CELL_COUNT) {
    return;
  }
  if (dir == Up) {
    walls_[a] &= ~WALL_UP;
    walls_[b] &= ~WALL_DOWN;
  } else if (dir == Right) {
    walls_[a] &= ~WALL_RIGHT;
    walls_[b] &= ~WALL_LEFT;
  } else if (dir == Down) {
    walls_[a] &= ~WALL_DOWN;
    walls_[b] &= ~WALL_UP;
  } else {
    walls_[a] &= ~WALL_LEFT;
    walls_[b] &= ~WALL_RIGHT;
  }
}
