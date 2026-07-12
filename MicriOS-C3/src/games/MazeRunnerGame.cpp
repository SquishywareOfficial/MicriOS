#include "MazeRunnerGame.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include "../../PlayerProfile.h"

namespace {
constexpr uint8_t WALL_UP = 1 << 0;
constexpr uint8_t WALL_RIGHT = 1 << 1;
constexpr uint8_t WALL_DOWN = 1 << 2;
constexpr uint8_t WALL_LEFT = 1 << 3;
constexpr int GRID_X = 3;
constexpr int GRID_Y = 7;
constexpr int CELL_W = 7;
constexpr int CELL_H = 6;
Preferences mazePrefs;
}

MazeRunnerGame::MazeRunnerGame(uint32_t width, uint32_t height, uint32_t left, bool collectorMode)
    : App(collectorMode ? "Maze Collector" : "Maze Runner", width, height), left_(left), collectorMode_(collectorMode) {}

bool MazeRunnerGame::hasCustomOverlay() const {
  return true;
}

void MazeRunnerGame::onAppReset() {
  loadBestLevel();
  level_ = 1;
  nextLevel();
}

void MazeRunnerGame::updateRunning(uint32_t deltaMs, const ButtonInput& input) {
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
    if (input.click && optionCount_ > 0) {
      selectedOption_ = (selectedOption_ + 1) % optionCount_;
    }
    if ((input.down && input.holdMs > 300) || input.longPress) {
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

void MazeRunnerGame::drawRunning(U8G2& u8g2) {
  u8g2.drawFrame(left_, 0, width, height);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(left_ + 2, 6);
  u8g2.print("L");
  u8g2.print(level_);
  u8g2.print(" T");
  u8g2.print(timeLeftMs_ / 1000);
  if (collectorMode_) {
    u8g2.print(" K");
    u8g2.print(pickupCount_ - __builtin_popcount(collectedMask_));
  }

  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      const uint8_t cell = r * COLS + c;
      const int x = left_ + GRID_X + c * CELL_W;
      const int y = GRID_Y + r * CELL_H;
      if (walls_[cell] & WALL_UP) {
        u8g2.drawLine(x, y, x + CELL_W, y);
      }
      if (walls_[cell] & WALL_LEFT) {
        u8g2.drawLine(x, y, x, y + CELL_H);
      }
      if (walls_[cell] & WALL_RIGHT) {
        u8g2.drawLine(x + CELL_W, y, x + CELL_W, y + CELL_H);
      }
      if (walls_[cell] & WALL_DOWN) {
        u8g2.drawLine(x, y + CELL_H, x + CELL_W, y + CELL_H);
      }
    }
  }

  const uint8_t runnerCol = runnerCell_ % COLS;
  const uint8_t runnerRow = runnerCell_ / COLS;
  const int runnerX = left_ + GRID_X + runnerCol * CELL_W + 3;
  const int runnerY = GRID_Y + runnerRow * CELL_H + 3;
  if (collectorMode_) {
    for (uint8_t i = 0; i < pickupCount_; i++) {
      if ((collectedMask_ & (1 << i)) != 0) {
        continue;
      }
      const uint8_t pickupCol = pickupCells_[i] % COLS;
      const uint8_t pickupRow = pickupCells_[i] / COLS;
      const int pickupX = left_ + GRID_X + pickupCol * CELL_W + 3;
      const int pickupY = GRID_Y + pickupRow * CELL_H + 3;
      u8g2.drawCircle(pickupX, pickupY, 2);
      u8g2.drawPixel(pickupX, pickupY);
    }
  }
  u8g2.drawBox(runnerX, runnerY - 1, 2, 2);
  if (runnerState_ == RunnerState::Choosing && optionCount_ > 0) {
    const Direction option = options_[selectedOption_];
    int x2 = runnerX;
    int y2 = runnerY;
    if (option == Up) {
      y2 -= 4;
    } else if (option == Right) {
      x2 += 5;
    } else if (option == Down) {
      y2 += 4;
    } else {
      x2 -= 5;
    }
    u8g2.drawLine(runnerX, runnerY, x2, y2);
  }
  const int exitX = left_ + GRID_X + (COLS - 1) * CELL_W + 2;
  const int exitY = GRID_Y + (ROWS - 1) * CELL_H + 1;
  u8g2.drawFrame(exitX, exitY, 4, 4);
  if (collectorMode_ && !exitUnlocked()) {
    u8g2.drawLine(exitX, exitY, exitX + 3, exitY + 3);
    u8g2.drawLine(exitX + 3, exitY, exitX, exitY + 3);
  }
}

void MazeRunnerGame::drawStart(U8G2& u8g2) {
  loadBestLevel();
  u8g2.drawFrame(0, 0, width + 2, height);
  if (showStartPromptPage()) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(20, 16, "Press");
    u8g2.drawStr(13, 29, "to Start");
  } else if (showStartScorePage()) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 10, "Top Score");
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(3, 24);
    if (bestLevel_ == 0) {
      u8g2.print("--");
    } else {
      u8g2.print(initials);
      u8g2.print(" L");
      u8g2.print(bestLevel_);
    }
  } else {
    u8g2.setFont(collectorMode_ ? u8g2_font_4x6_tr : u8g2_font_5x8_tr);
    u8g2.drawStr(3, 10, appTitle());
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(3, 21, collectorMode_ ? "Get keys" : "Tap choose");
    u8g2.drawStr(3, 29, "Hold run");
  }
}

void MazeRunnerGame::drawEnd(U8G2& u8g2) {
  u8g2.drawFrame(0, 0, width + 2, height);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(3, 9, "Lost");
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(3, 20);
  u8g2.print("Level ");
  u8g2.print(level_);
  u8g2.setCursor(3, 29);
  u8g2.print("Best L");
  u8g2.print(bestLevel_);
  if (bestLevel_ > 0) {
    char initials[4];
    PlayerProfile::unpackDottedInitials(bestInitials_, initials);
    u8g2.print(" ");
    u8g2.print(initials);
  }
  u8g2.drawStr(3, 38, "Tap retry Hold menu");
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
