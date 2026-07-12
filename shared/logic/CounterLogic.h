#pragma once

#include <stdint.h>

class CounterLogic {
public:
    CounterLogic() : count_(0) {}

    void increment() { count_++; }
    void reset() { count_ = 0; }
    uint32_t getCount() const { return count_; }
    void setCount(uint32_t count) { count_ = count; }

private:
    uint32_t count_;
};
