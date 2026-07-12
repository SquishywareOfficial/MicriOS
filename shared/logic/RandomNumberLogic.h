#pragma once

#include <stdint.h>
#include <Arduino.h>

class RandomNumberLogic {
public:
    enum class Mode {
        IncludeZero,
        Range,
        Result
    };

    static constexpr uint8_t RANGE_COUNT = 18;

    RandomNumberLogic()
        : mode_(Mode::IncludeZero),
          includeZero_(false),
          rangeIndex_(0),
          result_(0),
          rolled_(false) {}

    void reset() {
        mode_ = Mode::IncludeZero;
        rangeIndex_ = 0;
        rolled_ = false;
    }

    void update(uint32_t deltaMs, bool click, bool longPress) {
        (void)deltaMs;
        if (mode_ == Mode::IncludeZero) {
            if (click) includeZero_ = !includeZero_;
            if (longPress) mode_ = Mode::Range;
            return;
        }

        if (mode_ == Mode::Range) {
            if (click) {
                rangeIndex_ = (rangeIndex_ + 1) % RANGE_COUNT;
            }
            if (longPress) {
                roll();
            }
            return;
        }

        if (click) roll();
        if (longPress) {
            mode_ = Mode::IncludeZero;
            rolled_ = false;
        }
    }

    void roll() {
        result_ = random(getMin(), getMax() + 1);
        rolled_ = true;
        mode_ = Mode::Result;
    }

    static uint32_t getRangeAt(uint8_t index) {
        static const uint32_t ranges[RANGE_COUNT] = {
            10, 20, 30, 40, 50, 60, 70, 80, 90, 100,
            200, 300, 400, 500, 1000, 10000, 100000, 1000000
        };
        return ranges[index % RANGE_COUNT];
    }

    uint32_t getMin() const { return includeZero_ ? 0 : 1; }
    uint32_t getMax() const { return getRangeAt(rangeIndex_); }
    uint32_t getResult() const { return result_; }
    uint8_t getRangeIndex() const { return rangeIndex_; }
    bool includesZero() const { return includeZero_; }
    bool isRolled() const { return rolled_; }
    bool isEditing() const { return mode_ != Mode::Result; }
    bool isChoosingIncludeZero() const { return mode_ == Mode::IncludeZero; }
    bool isChoosingRange() const { return mode_ == Mode::Range; }
    Mode getMode() const { return mode_; }

private:
    Mode mode_;
    bool includeZero_;
    uint8_t rangeIndex_;
    uint32_t result_;
    bool rolled_;
};
