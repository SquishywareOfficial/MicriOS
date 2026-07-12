#pragma once

#include <stdint.h>
#include <Arduino.h>

class MouseEmulatorLogic {
public:
    enum MovePhase { Idle = 0, Forward = 1, Waiting = 2, Back = 3 };

    MouseEmulatorLogic() :
        jigglerEnabled_(true),
        isMoving_(false),
        phase_(Idle),
        lastJiggleTime_(0),
        targetIntervalMs_(0),
        movementTotal_(0),
        remainingSteps_(0),
        movementDirection_(1),
        waitUntilMs_(0),
        verticalOffset_(0),
        verticalJitterCountdown_(0) {}

    void reset(uint32_t nowMs) {
        lastJiggleTime_ = nowMs;
        calculateNextInterval();
        jigglerEnabled_ = true;
        isMoving_ = false;
        phase_ = Idle;
        movementTotal_ = 0;
        remainingSteps_ = 0;
        movementDirection_ = 1;
        waitUntilMs_ = 0;
        verticalOffset_ = 0;
        verticalJitterCountdown_ = 0;
    }

    void calculateNextInterval() {
        targetIntervalMs_ = random(10, 51) * 1000;
    }

    void toggleEnabled(uint32_t nowMs) {
        jigglerEnabled_ = !jigglerEnabled_;
        if (jigglerEnabled_) {
            lastJiggleTime_ = nowMs;
            calculateNextInterval();
        }
    }

    bool shouldStartMove(uint32_t nowMs, bool connected) {
        return connected && jigglerEnabled_ && phase_ == Idle && (nowMs - lastJiggleTime_) >= targetIntervalMs_;
    }

    void startMove(uint32_t nowMs, int minMove, int maxMove) {
        movementTotal_ = random(minMove, maxMove + 1);
        remainingSteps_ = movementTotal_;
        movementDirection_ = 1;
        verticalOffset_ = 0;
        phase_ = Forward;
        isMoving_ = true;
    }

    // State getters
    bool isEnabled() const { return jigglerEnabled_; }
    bool isMoving() const { return isMoving_; }
    MovePhase getPhase() const { return phase_; }
    uint32_t getLastJiggleTime() const { return lastJiggleTime_; }
    uint32_t getTargetIntervalMs() const { return targetIntervalMs_; }
    int getLastMovementPixels() const { return movementTotal_; }

    // Mutable state for the app to manage transitions if needed
    void setPhase(MovePhase p) { phase_ = p; }
    void setMoving(bool m) { isMoving_ = m; }
    void setLastJiggleTime(uint32_t t) { lastJiggleTime_ = t; }
    void setWaitUntil(uint32_t t) { waitUntilMs_ = t; }
    uint32_t getWaitUntil() const { return waitUntilMs_; }

    int& remainingSteps() { return remainingSteps_; }
    int& movementDirection() { return movementDirection_; }
    int& verticalOffset() { return verticalOffset_; }
    int& verticalJitterCountdown() { return verticalJitterCountdown_; }

private:
    bool jigglerEnabled_;
    bool isMoving_;
    MovePhase phase_;
    uint32_t lastJiggleTime_;
    uint32_t targetIntervalMs_;
    int movementTotal_;
    int remainingSteps_;
    int movementDirection_;
    uint32_t waitUntilMs_;
    int verticalOffset_;
    int verticalJitterCountdown_;
};
