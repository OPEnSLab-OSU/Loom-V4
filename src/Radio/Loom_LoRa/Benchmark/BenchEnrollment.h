#pragma once

#include <stdint.h>

namespace bench {

using BoardKey = uint64_t;
constexpr uint8_t BoardKeyBytes = 8, JoinSlots = 4;

// Names and requested device numbers can change; the hardware serial is stable.
inline BoardKey boardFingerprint(const char *serial) {
    if (!serial || !*serial) {
        return 0;
    }
    BoardKey key = 14695981039346656037ULL;
    for (; *serial; ++serial) {
        key ^= static_cast<uint8_t>(*serial);
        key *= 1099511628211ULL;
    }
    return key ? key : 1; // Zero represents an empty slot.
}

inline void writeBoardKey(uint8_t *bytes, BoardKey key) {
    for (uint8_t i = 0; i < BoardKeyBytes; ++i) {
        bytes[i] = static_cast<uint8_t>(key >> (8 * i));
    }
}

inline BoardKey readBoardKey(const uint8_t *bytes) {
    BoardKey key = 0;
    for (uint8_t i = 0; i < BoardKeyBytes; ++i) {
        key |= BoardKey(bytes[i]) << (8 * i);
    }
    return key;
}

inline uint8_t joinBucket(BoardKey key, uint16_t window) {
    // Use a different two-bit group each window. Distinct keys that collide now
    // will choose different slots in at least one of the next 32 windows.
    const uint8_t shift = static_cast<uint8_t>(2 * (window % (BoardKeyBytes * 4)));
    return static_cast<uint8_t>(((key >> shift) ^ window) % JoinSlots);
}

template <uint8_t Capacity>
class EnrollmentSlots {
    static_assert(Capacity > 0 && Capacity < UINT8_MAX, "Slot IDs must fit in one byte");

  public:
    BoardKey keyAt(uint8_t slot) const {
        return slot && slot <= Capacity ? keys[slot - 1] : 0;
    }

    uint8_t highestReserved() const {
        for (uint8_t slot = Capacity; slot > 0; --slot) {
            if (keyAt(slot)) {
                return slot;
            }
        }
        return 0;
    }

    uint8_t assign(BoardKey key, uint8_t preferred, uint8_t limit) {
        if (!key || !limit || limit > Capacity) {
            return 0;
        }
        for (uint8_t slot = 1; slot <= Capacity; ++slot) {
            if (keyAt(slot) == key) {
                return slot <= limit ? slot : 0;
            }
        }
        if (preferred && preferred <= limit && !keyAt(preferred)) {
            keys[preferred - 1] = key;
            return preferred;
        }
        for (uint8_t slot = 1; slot <= limit; ++slot) {
            if (!keyAt(slot)) {
                keys[slot - 1] = key;
                return slot;
            }
        }
        return 0;
    }

    bool forget(uint8_t slot) {
        if (!keyAt(slot)) {
            return false;
        }
        keys[slot - 1] = 0;
        return true;
    }

  private:
    BoardKey keys[Capacity] = {};
};

} // namespace bench
