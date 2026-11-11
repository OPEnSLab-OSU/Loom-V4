#pragma once

#include <stdint.h>

namespace bench {

constexpr uint8_t MaxNodes = 20;
using SlotMask = uint32_t;
constexpr uint8_t SlotMaskBytes = (MaxNodes + 7) / 8;
constexpr uint8_t MaxRetries = 3;
constexpr uint16_t RetryLeadMs = 30;
constexpr uint8_t RetryAttemptOffset = 4 + SlotMaskBytes;
constexpr uint8_t RetryHeaderBytes = RetryAttemptOffset + 1;

inline SlotMask readSlotMask(const uint8_t *bytes) {
    SlotMask mask = 0;
    for (uint8_t i = 0; i < SlotMaskBytes; ++i) {
        mask |= SlotMask(bytes[i]) << (8 * i);
    }
    return mask;
}

inline void writeSlotMask(uint8_t *bytes, SlotMask mask) {
    for (uint8_t i = 0; i < SlotMaskBytes; ++i) {
        bytes[i] = static_cast<uint8_t>(mask >> (8 * i));
    }
}

constexpr SlotMask slotMask(uint8_t count) {
    return count <= MaxNodes ? (SlotMask(1) << count) - 1 : 0;
}

inline uint8_t slotCount(SlotMask mask) {
    uint8_t count = 0;
    for (; mask; mask &= mask - 1) {
        ++count;
    }
    return count;
}

inline uint8_t retrySlot(SlotMask mask, uint8_t node) {
    if (!node || node > MaxNodes || !(mask & (SlotMask(1) << (node - 1)))) {
        return 0;
    }
    return slotCount(mask & slotMask(node)); // 1-based position among requested nodes.
}

constexpr uint8_t fragmentMask(uint8_t count) {
    return count <= 8 ? static_cast<uint8_t>((uint16_t(1) << count) - 1) : 0;
}

struct FragmentSet {
    uint8_t count, received;

    bool matches(uint8_t index, uint8_t total) const {
        return total <= 8 && index < total && (!count || count == total);
    }

    bool add(uint8_t index, uint8_t total) {
        if (!matches(index, total) || (received & (1u << index))) {
            return false;
        }
        count = total;
        received |= static_cast<uint8_t>(1u << index);
        return true;
    }

    bool complete() const { return count && received == fragmentMask(count); }
    uint8_t missing() const {
        return count ? static_cast<uint8_t>(fragmentMask(count) & ~received) : UINT8_MAX;
    }
};

} // namespace bench
