#pragma once

#include <stdint.h>
#include <Arduino.h>

class DiceRollerLogic {
public:
    enum class Mode { Select, Roll };

    DiceRollerLogic() : mode_(Mode::Select), selectedIdx_(1), result_(1), animMs_(0), rolling_(false) {}

    static const uint8_t DICE[];
    static const uint8_t DICE_COUNT;

    void reset() {
        mode_ = Mode::Select;
        rolling_ = false;
        animMs_ = 0;
    }

    void update(uint32_t deltaMs, bool click, bool longPress) {
        if (mode_ == Mode::Select) {
            if (click) {
                selectedIdx_ = (selectedIdx_ + 1) % DICE_COUNT;
            }
            if (longPress) {
                startRolling();
            }
        } else if (mode_ == Mode::Roll) {
            if (rolling_) {
                animMs_ += deltaMs;
                if (animMs_ >= 800) {
                    rolling_ = false;
                    result_ = random(1, DICE[selectedIdx_] + 1);
                }
            } else {
                if (click) {
                    startRolling();
                }
                if (longPress) {
                    mode_ = Mode::Select;
                }
            }
        }
    }

    void startRolling() {
        mode_ = Mode::Roll;
        rolling_ = true;
        animMs_ = 0;
    }

    Mode getMode() const { return mode_; }
    uint8_t getSelectedDice() const { return DICE[selectedIdx_]; }
    uint8_t getResult() const { return result_; }
    bool isRolling() const { return rolling_; }
    uint16_t getAnimMs() const { return animMs_; }

private:
    Mode mode_;
    uint8_t selectedIdx_;
    uint8_t result_;
    uint16_t animMs_;
    bool rolling_;
};

inline const uint8_t DiceRollerLogic::DICE[] = {4, 6, 8, 10, 12, 20, 100};
inline const uint8_t DiceRollerLogic::DICE_COUNT = sizeof(DICE) / sizeof(DICE[0]);
