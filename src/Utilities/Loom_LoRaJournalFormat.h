#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include "Loom_SDUtils.h"

namespace loomLoRa {
namespace journal {
inline uint32_t read32(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

inline void write32(uint8_t *p, uint32_t value) {
    for (uint8_t i = 0; i < 4; ++i) {
        p[i] = static_cast<uint8_t>(value);
        value >>= 8;
    }
}

inline uint32_t checksum(const uint8_t *data, size_t size) {
    uint32_t crc = UINT32_MAX;
    while (size--) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320UL : 0);
        }
    }
    return ~crc;
}

inline bool validEntry(const uint8_t *entry, size_t size) {
    return size >= 4 && read32(entry + size - 4) == checksum(entry, size - 4);
}

constexpr uint8_t CursorSize = 8;

// A cursor is disposable metadata. Keep the preceding checkpoint if a full final
// entry tore. If neither checkpoint is usable, replay the queue from its start.
// A failed read leaves the original cursor file untouched.
template <typename FileType>
bool recoverCursor(FileType &file, uint32_t queueEnd, uint8_t queueRecordSize, uint32_t &cursor) {
    if (!file) {
        return false;
    }
    if (queueRecordSize == 0) {
        return loomSD::closeAfterWrite(file, false);
    }
    const uint32_t original = file.fileSize();
    uint32_t end = original - original % CursorSize;
    uint32_t keep = 0, offset = 0;
    for (uint8_t checked = 0; end != 0 && checked < 2; ++checked) {
        uint8_t entry[CursorSize];
        if (!loomSD::readFixedRecord(file, end - CursorSize, entry, sizeof(entry))) {
            return loomSD::closeAfterWrite(file, false);
        }
        if (validEntry(entry, sizeof(entry))) {
            const uint32_t value = read32(entry);
            if (value <= queueEnd && value % queueRecordSize == 0) {
                offset = value;
                keep = end;
            }
            break;
        }
        end -= CursorSize;
    }
    const bool ready = keep == original || (file.truncate(keep) && file.sync());
    if (!loomSD::closeAfterWrite(file, ready)) {
        return false;
    }
    cursor = offset;
    return true;
}

enum class StoredReading { Match, Missing, Error };

// Hub journals are ordered by UTC and never insert older readings. A restored node can
// replay old entries, so verify the exact stored bytes before issuing a fresh receipt.
template <typename FileType>
StoredReading findStoredReading(FileType &file, uint32_t end, uint8_t size,
                                const uint8_t *expected, uint8_t *entry) {
    if (!file || size < 8 || end % size != 0) {
        return StoredReading::Error;
    }
    const uint32_t target = read32(expected);
    uint32_t first = 0;
    uint32_t last = end / size;
    while (first < last) {
        const uint32_t index = first + (last - first) / 2;
        if (!loomSD::readFixedRecord(file, index * size, entry, size) ||
            !validEntry(entry, size)) {
            return StoredReading::Error;
        }
        const uint32_t stamp = read32(entry);
        if (stamp == target) {
            return memcmp(entry, expected, size) == 0 ? StoredReading::Match
                                                      : StoredReading::Missing;
        }
        if (stamp < target) {
            first = index + 1;
        } else {
            last = index;
        }
    }
    return StoredReading::Missing;
}

constexpr uint8_t Network = 0xb7, Reading = 0x51, Receipt = 0x52;
constexpr uint8_t HeaderSize = 6;

inline void writeHeader(uint8_t *frame, uint8_t kind, uint32_t stamp) {
    frame[0] = Network;
    frame[1] = kind;
    write32(frame + 2, stamp);
}

} // namespace journal
} // namespace loomLoRa
