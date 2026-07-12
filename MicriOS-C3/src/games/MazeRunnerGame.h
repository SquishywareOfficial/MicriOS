#pragma once

#include "../../App.h"

class MazeRunnerGame : public App {
  public:
    MazeRunnerGame(uint32_t width, uint32_t height, uint32_t left, bool collectorMode = false);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

  private:
    static constexpr uint8_t COLS = 9;
    static constexpr uint8_t ROWS = 5;
    static constexpr uint8_t CELL_COUNT = COLS * ROWS;

    enum Direction {
      Up = 0,
      Right = 1,
      Down = 2,
      Left = 3
    };

    enum class RunnerState {
      Choosing,
      Moving
    };

    void loadBestLevel();
    void saveBestLevel();
    void generateMaze();
    void placePickups();
    void collectPickupAtRunner();
    void nextLevel();
    void collectOptions();
    void chooseDirection();
    void stepRunner();
    bool exitUnlocked() const;
    bool shouldStopForChoice() const;
    bool canMove(uint8_t cell, Direction dir) const;
    Direction turnRight(Direction dir) const;
    Direction turnLeft(Direction dir) const;
    Direction reverse(Direction dir) const;
    uint8_t neighborCell(uint8_t cell, Direction dir) const;
    void removeWall(uint8_t a, Direction dir);

    uint32_t left_;
    bool collectorMode_ = false;
    uint8_t walls_[CELL_COUNT] = {};
    uint8_t pickupCells_[3] = {};
    uint8_t pickupCount_ = 0;
    uint8_t collectedMask_ = 0;
    uint8_t runnerCell_ = 0;
    Direction dir_ = Right;
    RunnerState runnerState_ = RunnerState::Choosing;
    Direction options_[4] = {};
    uint8_t optionCount_ = 0;
    uint8_t selectedOption_ = 0;
    uint8_t level_ = 1;
    uint8_t bestLevel_ = 0;
    uint16_t bestInitials_ = 0;
    uint16_t stepTimerMs_ = 0;
    uint16_t timeLeftMs_ = 0;
    bool turnRequested_ = false;
    bool bestLoaded_ = false;
};
