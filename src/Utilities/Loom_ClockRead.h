#pragma once
#include "Loom_TimeUtils.h"

namespace loomTime {
////////////////////////////////////////////////////////////////////////////////////////////////////
// A plausible calendar is not enough: a stopped/reset oscillator can still return valid fields.
// Check both I2C operations and the RTC's oscillator-stop flag. Never modify output on failure.
// Templates keep this boundary independent of the RTC SDK and let host fakes inject each fault.
template <typename Clock, typename DateValue>
bool readEstablishedUtc(Clock &clock, DateValue &output) {
    const bool stopped = clock.lostPower();
    if (!clock.lastOperationSucceeded() || stopped) {
        return false;
    }
    const DateValue candidate = clock.now();
    if (!clock.lastOperationSucceeded() ||
        !validUtcFields(candidate.year(), candidate.month(), candidate.day(), candidate.hour(),
                        candidate.minute(), candidate.second())) {
        return false;
    }
    output = candidate;
    return true;
}

// A write ACK is not proof that the requested date was stored. Read back the UTC value and
// allow only the elapsed whole seconds plus one tick for crossing a clock-second boundary.
// The caller owns I2C/watchdog setup; this helper does not pause the watchdog or change alarms.
template <typename Clock, typename DateValue>
bool writeVerifiedUtc(Clock &clock, const DateValue &requested, uint32_t (*nowMs)()) {
    if (nowMs == nullptr) {
        return false;
    }
    const uint32_t started = nowMs();
    DateValue observed;
    if (!clock.adjustChecked(requested) || !readEstablishedUtc(clock, observed)) {
        return false;
    }
    const uint32_t elapsed = static_cast<uint32_t>(nowMs() - started);
    const uint32_t expectedUtc = requested.unixtime();
    const uint32_t actualUtc = observed.unixtime();
    return actualUtc >= expectedUtc && actualUtc - expectedUtc <= elapsed / 1000UL + 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace loomTime
