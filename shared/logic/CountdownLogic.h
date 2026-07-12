#pragma once

#include <stdint.h>

class CountdownLogic {
public:
    CountdownLogic() : durationMs_(60000), remainingMs_(60000), startTime_(0), running_(false) {}

    void reset() {
        remainingMs_ = durationMs_;
        running_ = false;
    }

    void toggle(uint32_t nowMs) {
        if (running_) {
            uint32_t elapsed = nowMs - startTime_;
            if (elapsed >= remainingMs_) {
                remainingMs_ = 0;
            } else {
                remainingMs_ -= elapsed;
            }
            running_ = false;
        } else {
            if (remainingMs_ > 0) {
                startTime_ = nowMs;
                running_ = true;
            }
        }
    }

    void adjustDuration(int32_t deltaMs) {
        if (running_) return;
        int32_t newDuration = (int32_t)durationMs_ + deltaMs;
        if (newDuration < 1000) newDuration = 1000;
        if (newDuration > 3600000) newDuration = 3600000; // 1 hour max
        durationMs_ = (uint32_t)newDuration;
        remainingMs_ = durationMs_;
    }

    uint32_t getRemainingMs(uint32_t nowMs) const {
        if (running_) {
            uint32_t elapsed = nowMs - startTime_;
            if (elapsed >= remainingMs_) return 0;
            return remainingMs_ - elapsed;
        }
        return remainingMs_;
    }

    bool isRunning() const { return running_; }
    uint32_t getDurationMs() const { return durationMs_; }

private:
    uint32_t durationMs_;
    uint32_t remainingMs_;
    uint32_t startTime_;
    bool running_;
};
