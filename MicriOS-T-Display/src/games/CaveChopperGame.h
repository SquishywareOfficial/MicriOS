#pragma once

#include "../../App.h"

class CaveChopperGame : public App {
  public:
    CaveChopperGame(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;

  private:
    static constexpr uint8_t SEGMENTS = 28;
    static constexpr uint8_t SEG_W = 10;
    static constexpr uint8_t GAP_H = 68;

    struct Segment {
      float x = 0.0f;
      int gapTop = 0;
      bool active = false;
    };

    int clampInt(int value, int minValue, int maxValue) const;
    int nextGapTop();
    void spawnSegment(float x);
    bool collides(const Segment& segment) const;
    void drawChopper(TFT_eSPI& tft, int x, int y, bool rotor) const;
    void loadBestScore();
    void saveBestScore();

    float y_ = 18.0f;
    float vy_ = 0.0f;
    float speed_ = 16.0f;
    uint32_t scoreMs_ = 0;
    uint16_t score_ = 0;
    uint16_t bestScore_ = 0;
    uint16_t bestInitials_ = 0;
    uint16_t rotorMs_ = 0;
    uint16_t randState_ = 0x9231;
    int lastGapTop_ = 32;
    bool bestLoaded_ = false;
    bool rotorOn_ = false;
    Segment segments_[SEGMENTS] = {};
};
