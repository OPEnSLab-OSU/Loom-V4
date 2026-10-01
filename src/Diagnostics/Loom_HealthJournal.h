#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace loomHealth {
////////////////////////////////////////////////////////////////////////////////////////////////////
// Infrequent, explicit health checkpoints. No SD, heap, clock driver or flash SDK is owned here.
// The backend must provide TWO independently erasable slots and outlive this journal.
// Use only from the main loop, never an ISR. Supply stability is the sketch owner's decision.
constexpr uint32_t UNAVAILABLE = UINT32_MAX;
constexpr uint32_t MAX_SAVED_CHECKPOINTS = 1000;
enum class Phase : uint32_t {
    Unknown,
    Boot,
    Measuring,
    Packaging,
    Saving,
    Publishing,
    Sleep,
    Awake
};
// Human-readable names keep saved numeric phase codes useful in a beginner's sketch.
inline const char *phaseName(Phase phase) {
    switch (phase) {
    case Phase::Boot:
        return "boot";
    case Phase::Measuring:
        return "measuring";
    case Phase::Packaging:
        return "packaging";
    case Phase::Saving:
        return "saving";
    case Phase::Publishing:
        return "publishing";
    case Phase::Sleep:
        return "sleep";
    case Phase::Awake:
        return "awake";
    default:
        return "unknown";
    }
}
struct Snapshot {
    Phase phase = Phase::Unknown;
    uint32_t uptimeMs = 0;
    uint32_t batteryMv = UNAVAILABLE;
    uint32_t freeRamBytes = UNAVAILABLE;
    uint32_t sensorMask = 0; // Sketch-defined bits; document which sensor each bit represents.
    uint32_t railConfig = UNAVAILABLE; // Configured rails, not a physical voltage measurement.
    uint32_t lteState = UNAVAILABLE;   // Sketch-defined code; no modem query during flash writes.
};
struct Record {
    uint32_t magic = 0, version = 0, sequence = 0, utc = 0;
    Snapshot health;
    uint32_t checksum = 0;
};
static_assert(sizeof(Record) == 48, "Health storage needs a fixed, pointer-free record layout.");

class Storage {
  public:
    virtual ~Storage() = default;
    virtual bool read(uint8_t slot, Record &output) = 0;
    virtual bool write(uint8_t slot, const Record &record) = 0;
};

enum class SaveResult {
    Saved,
    TooSoon,
    UnsafeSupply,
    StorageError,
    Corrupt,
    Exhausted,
    InvalidInput
};
inline const char *saveResultName(SaveResult result) {
    switch (result) {
    case SaveResult::Saved:
        return "checkpoint saved and verified";
    case SaveResult::TooSoon:
        return "checkpoint interval has not elapsed, or clock moved backward";
    case SaveResult::UnsafeSupply:
        return "writes disabled or supply not verified stable";
    case SaveResult::StorageError:
        return "storage or readback failed";
    case SaveResult::Corrupt:
        return "unrecognized/conflicting records preserved";
    case SaveResult::Exhausted:
        return "checkpoint sequence limit reached";
    case SaveResult::InvalidInput:
        return "invalid UTC, phase or checkpoint interval";
    }
    return "unknown checkpoint result";
}

class Journal {
  public:
    explicit Journal(Storage &storage, uint32_t minimumSeconds = 86400)
        : storage(storage), minimumSeconds(minimumSeconds) {}
    Journal(const Journal &) = delete;
    Journal &operator=(const Journal &) = delete;
    bool load(Record &output) {
        const Scan found = scan();
        if (found.latest < 0) {
            return false; // Leave the caller's output alone unless a valid checkpoint exists.
        }
        output = found.records[found.latest];
        return true;
    }
    SaveResult save(const Snapshot &health, uint32_t utc, bool supplyStable) {
        if (!supplyStable) {
            return SaveResult::UnsafeSupply; // Do not even begin erase/write on falling supply.
        }
        if (blocked) {
            return SaveResult::StorageError;
        }
        if (minimumSeconds < 3600 || minimumSeconds > 27UL * 86400UL || !validUtc(utc) ||
            static_cast<uint32_t>(health.phase) > static_cast<uint32_t>(Phase::Awake)) {
            return SaveResult::InvalidInput;
        }
        const Scan found = scan();
        if (!found.readable[0] || !found.readable[1]) {
            return SaveResult::StorageError; // Never overwrite a slot whose contents are unknown.
        }
        if (found.latest == CONFLICTING_RECORDS ||
            (found.latest < 0 && (!blank(found.records[0]) || !blank(found.records[1])))) {
            return SaveResult::Corrupt; // Preserve unrecognized data; no automatic format/erase.
        }
        uint32_t sequence = 1;
        uint8_t target = 0;
        if (found.latest >= 0) {
            const Record &previous = found.records[found.latest];
            if (previous.sequence >= MAX_SAVED_CHECKPOINTS) {
                return SaveResult::Exhausted;
            }
            if (utc < previous.utc || utc - previous.utc < minimumSeconds) {
                return SaveResult::TooSoon; // Applies across resets and clock corrections too.
            }
            sequence = previous.sequence + 1;
            target = static_cast<uint8_t>(1 - found.latest);
        }
        Record next;
        next.magic = MAGIC;
        next.version = 1;
        next.sequence = sequence;
        next.utc = utc;
        next.health = health;
        next.checksum = checksum(next);
        Record verified;
        // Keep the last valid slot intact. Read back the replacement before reporting success.
        if (!storage.write(target, next) || !storage.read(target, verified) || !valid(verified) ||
            std::memcmp(&next, &verified, sizeof(next)) != 0) {
            blocked = true; // No erase/retry loop after a failed write in this Journal lifetime.
            return SaveResult::StorageError;
        }
        return SaveResult::Saved;
    }
    static uint32_t checksum(const Record &record) {
        // FNV-1a detects accidental record damage; this is not authentication.
        uint32_t value = 2166136261UL;
        const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
        for (size_t i = 0; i < offsetof(Record, checksum); ++i) {
            value = (value ^ bytes[i]) * 16777619UL;
        }
        return value;
    }
    static bool valid(const Record &record) {
        return record.magic == MAGIC && record.version == 1 && record.sequence > 0 &&
               record.sequence <= MAX_SAVED_CHECKPOINTS && validUtc(record.utc) &&
               static_cast<uint32_t>(record.health.phase) <= static_cast<uint32_t>(Phase::Awake) &&
               record.checksum == checksum(record);
    }

  private:
    static constexpr uint32_t MAGIC = 0x4C484A31; // "LHJ1" record family.
    static constexpr int8_t NO_RECORD = -1, CONFLICTING_RECORDS = -2;
    static bool validUtc(uint32_t utc) { return utc >= 946684800UL && utc < 4102444800UL; }
    static bool blank(const Record &record) {
        const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
        const uint8_t emptyValue = bytes[0];
        if (emptyValue != 0 && emptyValue != 0xff) {
            return false;
        }
        for (size_t i = 1; i < sizeof(record); ++i) {
            if (bytes[i] != emptyValue) {
                return false;
            }
        }
        return true;
    }
    struct Scan {
        Record records[2];
        bool readable[2] = {};
        int8_t latest = NO_RECORD;
    };
    Scan scan() {
        Scan found;
        bool usable[2];
        for (uint8_t slot = 0; slot < 2; ++slot) {
            found.readable[slot] = storage.read(slot, found.records[slot]);
            usable[slot] = found.readable[slot] && valid(found.records[slot]);
        }
        if (usable[0] && usable[1]) {
            if (found.records[0].sequence == found.records[1].sequence &&
                std::memcmp(&found.records[0], &found.records[1], sizeof(Record)) != 0) {
                found.latest = CONFLICTING_RECORDS; // Equal sequence, different data.
            } else {
                found.latest = found.records[1].sequence > found.records[0].sequence ? 1 : 0;
            }
        } else if (usable[0] || usable[1]) {
            found.latest = usable[0] ? 0 : 1;
        }
        return found;
    }
    Storage &storage;
    const uint32_t minimumSeconds;
    bool blocked = false;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace loomHealth
