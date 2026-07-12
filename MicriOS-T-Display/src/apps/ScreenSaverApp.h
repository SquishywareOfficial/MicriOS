#pragma once

#include "../../App.h"

class ScreenSaverApp : public App {
 public:
  ScreenSaverApp(uint32_t width, uint32_t height);

  uint16_t runningRenderIntervalMs() const override;
  bool hasCustomOverlay() const override;

 protected:
  void onAppReset() override;
  void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
  void drawRunning(TFT_eSPI& tft) override;
  bool startsRunningImmediately() const override;

 private:
  enum class Mode : uint8_t { Select, Play };
  enum class Saver : uint8_t {
    Stars,
    Pipes,
    Stargate,
    Lines,
    Fractal,
    Text,
    Matrix,
    Plasma,
    Fire,
    Balls,
    Ribbons,
    Lissajous,
    Hypercube,
    Snow,
    Fireworks,
    Grid,
    Radar,
    Kaleidoscope,
    Spirograph,
    Sandstorm,
    NightDrive,
    Count
  };

  struct Star {
    float x;
    float y;
    float z;
  };

  struct PipeSegment {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
    uint16_t color;
  };

  struct PipeRun {
    int16_t x;
    int16_t y;
    int8_t dir;
    uint16_t color;
    uint8_t steps;
    bool active;
  };

  struct Particle {
    float x;
    float y;
    float vx;
    float vy;
    uint8_t life;
    uint16_t color;
  };

  struct MatrixColumn {
    int16_t y;
    uint8_t speed;
    uint8_t length;
  };

  struct Ball {
    float x;
    float y;
    float vx;
    float vy;
    uint8_t r;
    uint16_t color;
  };

  const char* saverName(Saver saver) const;
  void resetAnimation();
  void resetStar(Star& star, bool farAway);
  void resetPipes();
  void resetParticles();
  void burstFirework(int16_t x, int16_t y);
  void drawSelect(TFT_eSPI& tft);
  template <typename Canvas>
  void drawFrame(Canvas& canvas);
  template <typename Canvas>
  void drawStars(Canvas& canvas);
  template <typename Canvas>
  void drawPipes(Canvas& canvas);
  template <typename Canvas>
  void drawPipeSegment(Canvas& canvas, const PipeSegment& seg);
  template <typename Canvas>
  void drawStargate(Canvas& canvas);
  template <typename Canvas>
  void drawLines(Canvas& canvas);
  template <typename Canvas>
  void drawFractal(Canvas& canvas);
  template <typename Canvas>
  void drawText(Canvas& canvas);
  template <typename Canvas>
  void drawMatrix(Canvas& canvas);
  template <typename Canvas>
  void drawPlasma(Canvas& canvas);
  template <typename Canvas>
  void drawFire(Canvas& canvas);
  template <typename Canvas>
  void drawBalls(Canvas& canvas);
  template <typename Canvas>
  void drawRibbons(Canvas& canvas);
  template <typename Canvas>
  void drawLissajous(Canvas& canvas);
  template <typename Canvas>
  void drawHypercube(Canvas& canvas);
  template <typename Canvas>
  void drawSnow(Canvas& canvas);
  template <typename Canvas>
  void drawFireworks(Canvas& canvas);
  template <typename Canvas>
  void drawGrid(Canvas& canvas);
  template <typename Canvas>
  void drawRadar(Canvas& canvas);
  template <typename Canvas>
  void drawKaleidoscope(Canvas& canvas);
  template <typename Canvas>
  void drawSpirograph(Canvas& canvas);
  template <typename Canvas>
  void drawSandstorm(Canvas& canvas);
  template <typename Canvas>
  void drawNightDrive(Canvas& canvas);

  Mode mode_ = Mode::Select;
  Saver selected_ = Saver::Stars;
  uint32_t elapsedMs_ = 0;
  uint16_t frame_ = 0;
  Star stars_[36];
  PipeSegment pipeSegments_[156];
  PipeRun pipeRuns_[6];
  uint8_t pipeSegmentCount_ = 0;
  uint8_t pipeRunCount_ = 0;
  uint16_t pipePauseFrame_ = 0;
  uint16_t pipeLastGrowthFrame_ = 0;
  bool pipePaused_ = false;
  MatrixColumn matrix_[24];
  Ball balls_[7];
  Particle particles_[60];
  int16_t textX_ = 18;
  int16_t textY_ = 56;
  int8_t textVx_ = 2;
  int8_t textVy_ = 1;
  bool fireworkPrimed_ = false;
  bool dirty_ = true;
};
