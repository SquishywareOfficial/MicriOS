#pragma once

#include "../../App.h"

class TinyGolfGame : public App {
  public:
    TinyGolfGame(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;

  private:
    static constexpr uint8_t HOLE_COUNT = 5;
    static constexpr int HUD_H = 7;
    static constexpr float BALL_R = 1.2f;

    enum class PlayState {
      Aim,
      Power,
      Rolling,
      Holed
    };

    void loadBestScore();
    void saveBestScore();
    void loadHole(uint8_t holeIndex);
    void updateAim(uint32_t deltaMs);
    void updatePower(uint32_t deltaMs);
    void updateBall(uint32_t deltaMs);
    template <typename Canvas> void drawCourse(Canvas& tft);
    template <typename Canvas> void drawHud(Canvas& tft);
    template <typename Canvas> void drawAim(Canvas& tft);
    template <typename Canvas> void drawPower(Canvas& tft);
    void startShot();
    void finishHole();
    void finishCourse();
    bool aimLineNearCup() const;
    bool hitsCup() const;
    void bounceBounds();
    void bounceWalls(float previousX, float previousY);
    void bounceGuides(float previousX, float previousY);

    PlayState playState_ = PlayState::Aim;
    uint8_t holeIndex_ = 0;
    uint8_t totalStrokes_ = 0;
    uint8_t holeStrokes_ = 0;
    uint8_t bestScore_ = 0;
    uint16_t bestInitials_ = 0;
    bool bestScoreLoaded_ = false;
    float ballX_ = 0.0f;
    float ballY_ = 0.0f;
    float ballVx_ = 0.0f;
    float ballVy_ = 0.0f;
    float aimAngle_ = 0.0f;
    float power_ = 0.25f;
    int powerDir_ = 1;
    uint16_t holedTimerMs_ = 0;
};
