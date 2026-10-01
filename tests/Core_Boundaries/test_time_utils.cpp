// Portable production-helper regressions. Assertions stay enabled in the host runner.
#include "../../src/Utilities/Loom_TimeUtils.h"
#include <assert.h>

int main() {
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
    return 0;
}
