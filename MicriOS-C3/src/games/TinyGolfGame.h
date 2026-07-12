#pragma once

#include "../../App.h"

class TinyGolfGame : public App {
  public:
    TinyGolfGame(uint32_t width, uint32_t height, uint32_t left);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

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
    void drawCourse(U8G2& u8g2);
    void drawHud(U8G2& u8g2);
    void drawAim(U8G2& u8g2);
    void drawPower(U8G2& u8g2);
    void startShot();
    void finishHole();
    void finishCourse();
    bool aimLineNearCup() const;
    bool hitsCup() const;
    void bounceBounds();
    void bounceWalls(float previousX, float previousY);
    void bounceGuides(float previousX, float previousY);

    uint32_t left_;
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
