#pragma once

#include <stdint.h>
#include <Arduino.h>

class CoinFlipperLogic {
public:
    enum class Mode { Ready, Flipping, Result };

    CoinFlipperLogic() : mode_(Mode::Ready), heads_(true), animMs_(0) {}

    void reset() {
        mode_ = Mode::Ready;
        animMs_ = 0;
    }

    void update(uint32_t deltaMs, bool click, bool longPress) {
        if (mode_ == Mode::Ready) {
            if (click || longPress) {
                startFlip();
            }
        } else if (mode_ == Mode::Flipping) {
            animMs_ += deltaMs;
            if (animMs_ >= 800) {
                mode_ = Mode::Result;
                heads_ = (random(0, 2) == 0);
            }
        } else if (mode_ == Mode::Result) {
            if (click) {
                startFlip();
            }
            if (longPress) {
                // Let the app handle exit
            }
        }
    }

    void startFlip() {
        mode_ = Mode::Flipping;
        animMs_ = 0;
    }

    Mode getMode() const { return mode_; }
    bool isHeads() const { return heads_; }
    uint16_t getAnimMs() const { return animMs_; }

private:
    Mode mode_;
    bool heads_;
    uint16_t animMs_;
};
