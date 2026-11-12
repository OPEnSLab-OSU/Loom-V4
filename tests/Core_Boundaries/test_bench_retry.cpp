#include <cassert>
#include "Radio/Loom_LoRa/Benchmark/BenchRetry.h"

int main() {
    using namespace bench;
    static_assert(MaxRetries == 10, "Benchmark permits ten retransmissions");
    assert(chooseIndividualReply(false, 0, 0, UINT8_MAX) == IndividualReply::Build);
    assert(chooseIndividualReply(false, 1, 0, UINT8_MAX) == IndividualReply::Build);
    assert(chooseIndividualReply(false, 1, 0, 0x02) == IndividualReply::Ignore);
    assert(chooseIndividualReply(true, 2, 1, 0x02) == IndividualReply::Resend);
    assert(chooseIndividualReply(true, 2, 2, UINT8_MAX) == IndividualReply::Ignore);
    assert(chooseIndividualReply(true, 0, 2, UINT8_MAX) == IndividualReply::Build);
    FragmentSet parts{};
    assert(!parts.complete() && parts.missing() == 0xff);
    assert(!parts.add(0, 0) && !parts.add(8, 9));
    assert(parts.add(2, 3)); // The last fragment arrived before the first two.
    assert(parts.missing() == 0b011 && !parts.complete());
    assert(!parts.add(2, 3) && !parts.add(0, 2)); // Duplicate and mismatched fragment count.
    assert(parts.add(0, 3) && parts.missing() == 0b010);
    assert(parts.add(1, 3) && parts.complete() && parts.missing() == 0);
    assert(!parts.add(1, 3) && parts.complete());

    assert(slotMask(0) == 0 && slotMask(16) == 0xffff);
    assert(slotMask(20) == 0xfffff && slotMask(21) == 0);
    const SlotMask targets = 0x98009; // Node IDs 1, 4, 16, 17, and 20.
    assert(slotCount(targets) == 5);
    assert(retrySlot(targets, 1) == 1 && retrySlot(targets, 4) == 2 && retrySlot(targets, 16) == 3);
    assert(retrySlot(targets, 17) == 4 && retrySlot(targets, 20) == 5);
    assert(!retrySlot(targets, 0) && !retrySlot(targets, 2) && !retrySlot(targets, 21));

    uint8_t retry[RetryHeaderBytes + 5] = {};
    retry[RetryAttemptOffset] = MaxRetries;
    retry[RetryHeaderBytes + 4] = 0b10;
    writeSlotMask(retry + 4, targets);
    assert(retry[4] == 0x09 && retry[5] == 0x80 && retry[6] == 0x09);
    const uint8_t lastSlot = retrySlot(readSlotMask(retry + 4), 20);
    assert(lastSlot == 5 && retry[RetryAttemptOffset] == 10);
    assert(retry[RetryHeaderBytes + lastSlot - 1] == 0b10);
    assert((fragmentMask(2) & UINT8_MAX) == 0b11); // Full retry request for a missing sample.
}
