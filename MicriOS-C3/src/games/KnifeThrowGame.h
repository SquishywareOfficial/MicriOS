#pragma once

#include "../../App.h"

class KnifeThrowGame : public App {
  public:
    KnifeThrowGame(uint32_t width, uint32_t height, uint32_t left);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& input) override;
    void drawRunning(U8G2& u8g2) override;
    void drawStart(U8G2& u8g2) override;
    void drawEnd(U8G2& u8g2) override;

  private:
    void loadBest();
    void saveBest();
    void throwKnife();
    void resolveThrow();
    void drawBoard(U8G2& u8g2, int cx, int cy);
    void drawPerson(U8G2& u8g2, int cx, int cy) const;
    void drawReticle(U8G2& u8g2, int cx, int cy) const;
    bool localPointHitsPerson(float lx, float ly) const;
    float rotateX(float lx, float ly) const;
    float rotateY(float lx, float ly) const;

    uint32_t left_;
    struct Knife {
      float lx = 0.0f;
      float ly = 0.0f;
    };

    float boardAngle_ = 0.0f;
    float boardSpeed_ = 1.6f;
    float reticleAngle_ = 0.0f;
    float reticleRadius_ = 8.0f;
    float reticleSpeed_ = 2.2f;
    float thrownScreenX_ = 0.0f;
    float thrownScreenY_ = 0.0f;
    uint16_t throwMs_ = 0;
    bool throwActive_ = false;
    bool hitPerson_ = false;
    Knife knives_[16] = {};
    uint8_t knifeCount_ = 0;
    uint16_t score_ = 0;
    uint16_t bestScore_ = 0;
    uint16_t bestInitials_ = 0;
    bool bestLoaded_ = false;
};
