#pragma once

#include "../../App.h"

class MicriFieldGame : public App {
  public:
    MicriFieldGame(uint32_t width, uint32_t height);
    bool hasCustomOverlay() const override;

  protected:
    void onAppReset() override;
    void updateRunning(uint32_t deltaMs, const ButtonInput& b1, const ButtonInput& b2) override;
    void drawRunning(TFT_eSPI& tft) override;
    void drawStart(TFT_eSPI& tft) override;
    void drawEnd(TFT_eSPI& tft) override;

  private:
    enum class FieldState {
      EventIntro,
      CueSequence,
      HurdleCue,
      HurdleHold,
      DirectionSelect,
      AngleSelect,
      JavelinHoldAngle,
      HighJumpHold,
      ThrowAnim,
      Result
    };

    enum class CueMode {
      Dash,
      LongRun,
      HammerPower,
      JavelinRun,
      HighBonus
    };

    enum class ResultUnit {
      Time,
      Distance,
      Height,
      Count
    };

    void loadRecords();
    void saveTotalRecord();
    void saveEventRecord(uint8_t eventIndex);
    void startEventIntro();
    void startAttempt();
    void startCueSequence(CueMode mode, uint8_t total, uint16_t targetMs);
    void finishCueSequence();
    void finishAttempt(bool qualified, uint16_t eventScore, uint16_t value, ResultUnit unit);
    void handleResultInput(const ButtonInput& b1, const ButtonInput& b2);
    void advanceEvent();
    void loseLifeOrRetry();
    void addScore(uint16_t amount);
    void setFieldLed(bool on);
    void updateCueLed();
    uint8_t timingQuality(int16_t errorMs);
    uint8_t angleQuality(float selected, float optimal, float tolerance) const;
    uint16_t qualifyingTarget() const;
    uint8_t maxAttemptsForEvent() const;
    bool isFieldEvent() const;
    const char* eventName(uint8_t index) const;
    template <typename Canvas> void drawScoreHeader(Canvas& tft);
    template <typename Canvas> void drawEventIntro(Canvas& tft);
    template <typename Canvas> void drawCueSequence(Canvas& tft);
    template <typename Canvas> void drawHurdles(Canvas& tft);
    template <typename Canvas> void drawDirectionSelect(Canvas& tft);
    template <typename Canvas> void drawAngleSelect(Canvas& tft);
    template <typename Canvas> void drawJavelinHoldAngle(Canvas& tft);
    template <typename Canvas> void drawHighJumpHold(Canvas& tft);
    template <typename Canvas> void drawThrowAnim(Canvas& tft);
    template <typename Canvas> void drawResult(Canvas& tft);
    template <typename Canvas> void drawStick(Canvas& tft, int x, int groundY, bool airborne);
    template <typename Canvas> void drawHurdle(Canvas& tft, int x, int groundY, bool fallen = false);
    template <typename Canvas> void drawAngleFan(Canvas& tft, int ox, int oy, float angleDeg);
    template <typename Canvas> void drawTrackSplash(Canvas& tft);

    FieldState state_ = FieldState::EventIntro;
    CueMode cueMode_ = CueMode::Dash;
    ResultUnit resultUnit_ = ResultUnit::Time;
    uint32_t stateTimerMs_ = 0;
    uint8_t eventIndex_ = 0;
    uint8_t round_ = 1;
    uint8_t attemptsRemaining_ = 5;
    bool pressureMode_ = false;
    bool roundHadZero_ = false;
    uint16_t cueElapsedMs_ = 0;
    uint16_t cueTargetMs_ = 450;
    uint16_t cueBaseTargetMs_ = 450;
    uint8_t cueDone_ = 0;
    uint8_t cueTotal_ = 0;
    uint16_t cueQualitySum_ = 0;
    int16_t lastCueErrorMs_ = 0;
    uint16_t messageTimerMs_ = 0;
    uint8_t lastQuality_ = 0;
    uint16_t runQuality_ = 0;
    uint8_t directionQuality_ = 0;
    uint8_t angleQuality_ = 0;
    uint8_t powerQuality_ = 0;
    uint8_t hurdleIndex_ = 0;
    uint8_t hurdlesCleared_ = 0;
    uint16_t hurdleScrollMs_ = 0;
    uint16_t hurdleHoldMs_ = 0;
    uint8_t hurdleTimingQuality_ = 0;
    float hurdleRunnerY_ = 0.0f;
    float hurdleVy_ = 0.0f;
    float hurdleX_ = 0.0f;
    float hurdleSpeed_ = 22.0f;
    bool hurdleScored_ = false;
    bool hurdleHit_ = false;
    bool hurdleJumped_ = false;
    bool holding_ = false;
    float selectorAngle_ = 20.0f;
    int8_t selectorDir_ = 1;
    float directionDeg_ = 0.0f;
    uint16_t throwAnimMs_ = 0;
    int16_t throwSignedDistanceCm_ = 0;
    uint16_t highHoldMs_ = 0;
    uint8_t highAngleQuality_ = 0;
    bool qualified_ = false;
    bool extraLifeEarned_ = false;
    uint16_t resultValue_ = 0;
    uint16_t resultScore_ = 0;
    uint16_t bestAttemptScore_ = 0;
    uint16_t bestAttemptValue_ = 0;
    uint32_t totalScore_ = 0;
    uint32_t nextExtraLifeAt_ = 1000;
    uint32_t bestTotalScore_ = 0;
    uint16_t bestEventScores_[6] = {};
    uint16_t bestInitials_ = 0;
    bool bestLoaded_ = false;
};
