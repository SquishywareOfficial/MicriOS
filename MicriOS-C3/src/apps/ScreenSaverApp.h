#pragma once

#include "../../App.h"

class ScreenSaverApp : public App {
 public:
  ScreenSaverApp(uint32_t width, uint32_t height, uint32_t left = 1);

  uint16_t runningRenderIntervalMs() const override;
  bool hasCustomOverlay() const override;

 protected:
  void onAppReset() override;
  void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
  void drawRunning(U8G2& u8g2) override;
  bool startsRunningImmediately() const override;

 private:
  enum class Mode : uint8_t { Select, Play };
  enum class Saver : uint8_t { Stars, Text, Lines, Matrix, Snow, Lissajous, Radar, Fireworks, Count };

  struct Star {
    int8_t x;
    int8_t y;
    uint8_t z;
  };

  struct Drop {
    int8_t x;
    int8_t y;
    uint8_t speed;
  };

  struct Spark {
    int8_t x;
    int8_t y;
    int8_t vx;
    int8_t vy;
    uint8_t life;
  };

  const char* saverName(Saver saver) const;
  void resetAnimation();
  void resetStar(Star& star);
  void burstFirework();
  void drawSelect(U8G2& u8g2);
  void drawStars(U8G2& u8g2);
  void drawText(U8G2& u8g2);
  void drawLines(U8G2& u8g2);
  void drawMatrix(U8G2& u8g2);
  void drawSnow(U8G2& u8g2);
  void drawLissajous(U8G2& u8g2);
  void drawRadar(U8G2& u8g2);
  void drawFireworks(U8G2& u8g2);

  Mode mode_ = Mode::Select;
  Saver selected_ = Saver::Stars;
  uint16_t frame_ = 0;
  Star stars_[16];
  Drop drops_[12];
  Spark sparks_[18];
  int8_t textX_ = 8;
  int8_t textY_ = 21;
  int8_t textVx_ = 1;
  int8_t textVy_ = 1;
};
