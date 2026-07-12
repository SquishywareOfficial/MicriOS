#pragma once

#include "../../App.h"

class PipeManiaGame : public App {
  public:
    PipeManiaGame(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;

  private:
    static constexpr uint8_t COLS = 7;
    static constexpr uint8_t ROWS = 5;
    static constexpr uint8_t CELL_COUNT = COLS * ROWS;

    enum PipeType {
      Empty = 0,
      Horizontal,
      Vertical,
      TurnUR,
      TurnRD,
      TurnDL,
      TurnLU
    };

    enum Direction {
      Up = 0,
      Right = 1,
      Down = 2,
      Left = 3
    };

    enum class PipeState {
      Build,
      Flow,
      Complete
    };

    void loadBestScore();
    void saveBestScore();
    void loadLevel();
    void cyclePiece();
    void placePiece();
    void updateCursorToPipeEnd();
    void advanceFlow();
    void finishLevel();
    uint16_t buildDelayMs() const;
    uint16_t flowDelayMs() const;
    uint8_t openings(PipeType pipe) const;
    bool hasOpening(PipeType pipe, Direction dir) const;
    Direction opposite(Direction dir) const;
    Direction nextDirection(PipeType pipe, Direction enteredFrom) const;
    bool stepCell(uint8_t cell, Direction dir, uint8_t& nextCell) const;
    template <typename Canvas>
    void drawPipe(Canvas& tft, int x, int y, PipeType pipe, bool filled);

    PipeType grid_[CELL_COUNT] = {};
    bool filled_[CELL_COUNT] = {};
    PipeState pipeState_ = PipeState::Build;
    PipeType currentPiece_ = Horizontal;
    uint8_t cursor_ = 0;
    uint8_t buildEndCell_ = 0;
    uint8_t flowCell_ = 0;
    Direction buildDir_ = Right;
    Direction flowDir_ = Right;
    uint8_t flowedCount_ = 0;
    uint8_t targetCount_ = 8;
    uint8_t level_ = 1;
    uint16_t bestScore_ = 0;
    uint16_t bestInitials_ = 0;
    uint16_t score_ = 0;
    uint16_t buildTimerMs_ = 0;
    uint16_t cursorTimerMs_ = 0;
    uint16_t flowTimerMs_ = 0;
    uint16_t completeTimerMs_ = 0;
    bool bestLoaded_ = false;
    bool placedThisHold_ = false;
    bool won_ = false;
};
