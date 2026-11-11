#pragma once

#include <stdint.h>

namespace bench {

constexpr uint32_t MinPeriodMs = 100, MaxPeriodMs = 86400000;
constexpr uint16_t LocalReadBudgetMs = 350, MaxQuietMs = 3000;

constexpr bool validPeriod(uint32_t period) {
    return period == 0 || (period >= MinPeriodMs && period <= MaxPeriodMs);
}

struct PeriodicDeadline {
    uint32_t period = 0;
    uint32_t next = 0;

    void reset(uint32_t start, uint32_t interval) {
        next = start;
        period = interval;
    }

    uint32_t waitMs(uint32_t now) const {
        if (!period) {
            return UINT32_MAX;
        }
        return static_cast<int32_t>(now - next) >= 0 ? 0 : next - now;
    }

    // Take at most one reading or slot now. Return the number of elapsed deadlines
    // so callers can count skipped intervals without sending catch-up bursts.
    uint32_t take(uint32_t now) {
        if (waitMs(now) != 0) {
            return 0;
        }
        const uint32_t elapsed = (now - next) / period + 1;
        next += elapsed * period;
        return elapsed;
    }
};

} // namespace bench
