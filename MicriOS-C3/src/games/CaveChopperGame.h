#pragma once

#include "../../App.h"

class CaveChopperGame : public App {
  public:
    CaveChopperGame(uint32_t width, uint32_t height, uint32_t left);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

  private:
    static constexpr uint8_t SEGMENTS = 15;
    static constexpr uint8_t SEG_W = 5;
    static constexpr uint8_t GAP_H = 25;

    struct Segment {
      float x = 0.0f;
      int gapTop = 0;
      bool active = false;
    };

    int clampInt(int value, int minValue, int maxValue) const;
    int nextGapTop();
    void spawnSegment(float x);
    bool collides(const Segment& segment) const;
    void drawChopper(U8G2& u8g2, int x, int y, bool rotor) const;
    void loadBestScore();
    void saveBestScore();

    uint32_t left_;
    float y_ = 18.0f;
    float vy_ = 0.0f;
    float speed_ = 16.0f;
    uint32_t scoreMs_ = 0;
    uint16_t score_ = 0;
    uint16_t bestScore_ = 0;
    uint16_t bestInitials_ = 0;
    uint16_t rotorMs_ = 0;
    uint16_t randState_ = 0x9231;
    int lastGapTop_ = 8;
    bool bestLoaded_ = false;
    bool rotorOn_ = false;
    Segment segments_[SEGMENTS] = {};
};
