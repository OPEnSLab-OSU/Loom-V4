// Portable production-helper regressions. Assertions stay enabled in the host runner.
#include "../../src/Utilities/Loom_TimeUtils.h"
#include <assert.h>

int main() {
    uint32_t packetTime = 77;
    assert(loomTime::packetUtcSeconds("2026-10-01T08:43:08Z", packetTime));
    const uint32_t firstRecord = packetTime;
    assert(loomTime::packetUtcSeconds("2026-10-01T08:45:57Z", packetTime));
    assert(packetTime - firstRecord == 169); // Check actual CSV timestamps, not wake alarms.
    const char *badTimes[] = {nullptr, "", "2026-10-01T01:43:08", "2026-10-01T08:43:08Zx",
                             "2026-02-30T08:43:08Z", "2026-10-01T08:6x:08Z"};
    for (const char *bad : badTimes) {
        const uint32_t before = packetTime;
        assert(!loomTime::packetUtcSeconds(bad, packetTime));
        assert(packetTime == before);
    }
    using loomTime::validUtcFields;
    assert(!loomTime::validRtcWakeDelay(0));
    assert(!loomTime::validRtcWakeDelay(-1));
    assert(loomTime::validRtcWakeDelay(1));
    assert(loomTime::validRtcWakeDelay(27L * 86400));
    assert(!loomTime::validRtcWakeDelay(27L * 86400 + 1));
    assert(!loomTime::validRtcWakeDelay(INT32_MAX));
    assert(validUtcFields(2000, 2, 29, 0, 0, 0));
    assert(validUtcFields(2024, 2, 29, 23, 59, 59));
    assert(validUtcFields(2099, 12, 31, 23, 59, 59));
    assert(!validUtcFields(2025, 2, 29, 0, 0, 0));
    assert(!validUtcFields(2026, 4, 31, 0, 0, 0));
    assert(!validUtcFields(1999, 12, 31, 0, 0, 0));
    assert(!validUtcFields(2100, 1, 1, 0, 0, 0));
    assert(!validUtcFields(2026, 257, 1, 0, 0, 0));
    assert(!validUtcFields(2026, 1, 257, 0, 0, 0));
    assert(!validUtcFields(2026, 1, 1, 256, 0, 0));
    assert(!validUtcFields(2026, 1, 1, 0, -1, 0));
    assert(!validUtcFields(2026, 1, 1, 0, 0, 60));

    int32_t total = 0;
    assert(loomTime::addIntervalPart(256, 3600, total));
    assert(total == 921600); // Wide hours must not wrap through an 8-bit constructor.
    assert(!loomTime::addIntervalPart(-1, 60, total));
    assert(total == 921600); // Failed additions leave the accumulated interval unchanged.
    assert(!loomTime::addIntervalPart(INT32_MAX, 86400, total));
    assert(total == 921600);
    total = 0;
    assert(loomTime::addIntervalPart(INT32_MAX, 1, total));
    assert(!loomTime::addIntervalPart(1, 1, total));
    assert(total == INT32_MAX);

    uint32_t target = 0;
    assert(loomTime::nextSampleTime(1000, 60, 0, target));
    assert(target == 1060); // First sample anchors the schedule before measurement work.
    assert(loomTime::nextSampleTime(1060, 60, target, target));
    assert(target == 1120); // Work duration does not become part of the next period.
    assert(loomTime::nextSampleTime(1070, 60, target, target));
    assert(target == 1120); // Early wake/retry retains the same future alarm.
    assert(loomTime::nextSampleTime(1245, 60, target, target));
    assert(target == 1300); // Long uploads skip missed slots without a catch-up loop.
    assert(loomTime::nextSampleTime(1300, 60, target, target));
    assert(target == 1360); // Exactly due means next slot, not an alarm already in the past.
    assert(!loomTime::nextSampleTime(1000, 0, target, target));
    assert(target == 1360);
    assert(!loomTime::nextSampleTime(UINT32_MAX - 1, 60, 0, target));
    assert(target == 1360);
    assert(!loomTime::nextSampleTime(UINT32_MAX, 60, UINT32_MAX, target));
    assert(target == 1360); // Rejected calculations never partially change the output.

    // Wisp log: first alarm at 06:55:50; restoration and logging finish at 06:56:24.
    // Schedule from the previous alarm, so the 34 seconds of work do not accumulate.
    const uint32_t firstWake = 6UL * 3600 + 55UL * 60 + 50;
    target = firstWake;
    assert(loomTime::nextSampleTime(firstWake + 34, 180, target, target));
    assert(target == firstWake + 180);
    assert(target - (firstWake + 34) == 146);
    for (uint32_t cycle = 2; cycle <= 100; ++cycle) {
        const uint32_t finishedWork = target + 32 + cycle % 3;
        assert(loomTime::nextSampleTime(finishedWork, 180, target, target));
        assert(target == firstWake + cycle * 180); // Variable work never moves the grid.
    }
    assert(loomTime::nextSampleTime(target + 400, 180, target, target));
    assert(target == firstWake + 103UL * 180); // Skip two missed deadlines after a slow cycle.

    // Changing the SD interval after waking must not add the just-completed work to the
    // first new interval. Legacy callers still get a fresh phase on interval changes.
    const uint32_t lastWake = target;
    uint32_t anchor = loomTime::sampleIntervalAnchor(lastWake + 34, lastWake,
        lastWake - 146, 180, 600, true);
    assert(anchor == lastWake);
    assert(loomTime::nextSampleTime(lastWake + 34, 600, anchor, target));
    assert(target == lastWake + 600);
    assert(loomTime::nextSampleTime(lastWake + 1234, 600, anchor, target));
    assert(target == lastWake + 1800); // Overruns skip slots, not an accumulating work delay.
    assert(loomTime::sampleIntervalAnchor(lastWake + 34, lastWake, lastWake - 146,
        180, 600, false) == 0);
    assert(loomTime::sampleIntervalAnchor(lastWake + 34, lastWake + 180, lastWake,
        180, 600, true) == 0); // Do not treat an old future alarm as an actual wake.
    assert(loomTime::sampleIntervalAnchor(lastWake - 1, lastWake - 180, lastWake,
        180, 600, true) == 0); // Backwards RTC correction reanchors even in opt-in mode.
    assert(loomTime::sampleIntervalAnchor(lastWake + 34, lastWake + 180, lastWake,
        180, 180, true) == lastWake + 180); // Same interval retains an early/retry alarm.
    return 0;
}
