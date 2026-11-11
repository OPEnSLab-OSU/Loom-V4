#include <cassert>
#include "Radio/Loom_LoRa/Benchmark/BenchCadence.h"
#include "Radio/Loom_LoRa/Benchmark/BenchRetry.h"

int main() {
    using namespace bench;
    assert(validPeriod(0) && !validPeriod(99) && validPeriod(100));
    assert(validPeriod(86400000) && !validPeriod(86400001));

    PeriodicDeadline minute, fiveMinutes, disabled;
    minute.reset(1000, 60000);
    fiveMinutes.reset(1000, 300000);
    assert(disabled.take(1000) == 0 && disabled.waitMs(1000) == UINT32_MAX);
    assert(minute.take(1000) == 1 && fiveMinutes.take(1000) == 1);
    assert(minute.take(60999) == 0 && fiveMinutes.take(61000) == 0);
    assert(minute.take(61250) == 1 && minute.next == 121000); // A retry didn't shift phase.
    assert(minute.take(242000) == 3 && minute.next == 301000); // One slot, two skipped.
    assert(minute.take(301000) == 1 && fiveMinutes.take(301000) == 1);

    PeriodicDeadline wrapped;
    wrapped.reset(UINT32_MAX - 49, 100);
    assert(wrapped.take(UINT32_MAX - 49) == 1 && wrapped.next == 50);
    assert(wrapped.waitMs(UINT32_MAX - 9) == 60 && wrapped.take(49) == 0);
    assert(wrapped.take(50) == 1 && wrapped.next == 150);

    const SlotMask due = SlotMask(1) << 19; // Only node 20 is due; it takes position 1.
    uint8_t pacedMask[SlotMaskBytes];
    writeSlotMask(pacedMask, due);
    assert(readSlotMask(pacedMask) == due);
    assert(slotCount(due) == 1 && retrySlot(due, 20) == 1 && retrySlot(due, 1) == 0);
}
