#pragma once

#include <stdint.h>

class MetronomeLogic {
public:
    MetronomeLogic() : bpm_(120), running_(false), lastTickMs_(0) {}

    void reset() {
        running_ = false;
    }

    void toggle() {
        running_ = !running_;
        lastTickMs_ = 0;
    }

    void setBpm(uint16_t bpm) {
        if (bpm < 20) bpm = 20;
        if (bpm > 300) bpm = 300;
        bpm_ = bpm;
    }

    uint16_t getBpm() const { return bpm_; }

    bool update(uint32_t nowMs) {
        if (!running_) return false;
        uint32_t interval = 60000 / bpm_;
        if (lastTickMs_ == 0 || (nowMs - lastTickMs_) >= interval) {
            lastTickMs_ = nowMs;
            return true;
        }
        return false;
    }

    bool isRunning() const { return running_; }

private:
    uint16_t bpm_;
    bool running_;
    uint32_t lastTickMs_;
};
