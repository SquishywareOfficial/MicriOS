#pragma once

#include <stdint.h>

class BlackjackLogic {
public:
    enum class Phase { Bet, Deal, Player, Dealer, End };

    BlackjackLogic() : bank_(1000), bet_(10), phase_(Phase::Bet) {}

    void reset() {
        bank_ = 1000;
        bet_ = 10;
        phase_ = Phase::Bet;
    }

    void update(uint32_t deltaMs, bool click, bool longPress) {
        if (phase_ == Phase::Bet) {
            if (click) {
                bet_ += 10;
                if (bet_ > bank_) bet_ = 10;
            }
            if (longPress) {
                phase_ = Phase::Deal;
            }
        } else if (phase_ == Phase::End) {
            if (click || longPress) phase_ = Phase::Bet;
        }
    }

    int getBank() const { return bank_; }
    int getBet() const { return bet_; }
    Phase getPhase() const { return phase_; }

private:
    int bank_;
    int bet_;
    Phase phase_;
};
