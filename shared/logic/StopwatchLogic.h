#pragma once

#include <stdint.h>

class StopwatchLogic {
public:
    StopwatchLogic() : startTime_(0), elapsedMs_(0), running_(false) {}

    void reset() {
        startTime_ = 0;
        elapsedMs_ = 0;
        running_ = false;
    }

    void toggle(uint32_t nowMs) {
        if (running_) {
            elapsedMs_ += (nowMs - startTime_);
            running_ = false;
        } else {
            startTime_ = nowMs;
            running_ = true;
        }
    }

    uint32_t getElapsedMs(uint32_t nowMs) const {
        if (running_) {
            return elapsedMs_ + (nowMs - startTime_);
        }
        return elapsedMs_;
    }

    bool isRunning() const { return running_; }

private:
    uint32_t startTime_;
    uint32_t elapsedMs_;
    bool running_;
};
