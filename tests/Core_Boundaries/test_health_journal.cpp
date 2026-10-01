#include <cassert>
#include <cstring>
#include "Diagnostics/Loom_HealthJournal.h"
using namespace loomHealth;

class FakeStorage : public Storage {
  public:
    uint8_t bytes[2][sizeof(Record)] = {};
    int failAfter = -1, unreadableSlot = -1;
    unsigned int writes = 0;
    bool read(uint8_t slot, Record &output) override {
        if (slot > 1 || slot == unreadableSlot) {
            return false;
        }
        std::memcpy(&output, bytes[slot], sizeof(output));
        return true;
    }
    bool write(uint8_t slot, const Record &record) override {
        ++writes;
        if (slot > 1) {
            return false;
        }
        std::memset(bytes[slot], 0xff, sizeof(Record));
        const size_t count = failAfter < 0 ? sizeof(Record) : static_cast<size_t>(failAfter);
        std::memcpy(bytes[slot], &record, count);
        return failAfter < 0;
    }
};

int main() {
    constexpr uint32_t now = 1700000000UL, day = 86400;
    Snapshot first;
    first.phase = Phase::Awake;
    first.batteryMv = 4200;
    first.sensorMask = 3;
    FakeStorage storage;
    Journal journal(storage);
    Record saved;
    saved.sequence = 99;
    assert(!journal.load(saved) && saved.sequence == 99);
    assert(journal.save(first, now, false) == SaveResult::UnsafeSupply && storage.writes == 0);
    assert(journal.save(first, 0, true) == SaveResult::InvalidInput && storage.writes == 0);
    assert(journal.save(first, now, true) == SaveResult::Saved);
    assert(journal.load(saved) && saved.sequence == 1 && saved.health.batteryMv == 4200);
    assert(Journal::valid(saved));
    assert(journal.save(first, now + day - 1, true) == SaveResult::TooSoon);
    assert(journal.save(first, now - 1, true) == SaveResult::TooSoon);
    Snapshot second = first;
    second.phase = Phase::Publishing;
    assert(journal.save(second, now + day, true) == SaveResult::Saved);
    storage.bytes[1][20] ^= 1; // Damage newest slot: load the intact prior checkpoint.
    assert(journal.load(saved) && saved.sequence == 1);
    storage.unreadableSlot = 1;
    assert(journal.save(second, now + day, true) == SaveResult::StorageError);
    assert(storage.writes == 2);

    for (size_t cut = 0; cut < sizeof(Record); ++cut) {
        FakeStorage interrupted;
        Journal writer(interrupted);
        assert(writer.save(first, now, true) == SaveResult::Saved);
        interrupted.failAfter = static_cast<int>(cut);
        assert(writer.save(second, now + day, true) == SaveResult::StorageError);
        assert(writer.load(saved) && Journal::valid(saved));
        // A partial write may already contain every required bit. Accept only a whole valid
        // old or whole valid new record, never a mixture relabeled as the requested snapshot.
        assert(
            (saved.sequence == 1 && saved.utc == now && saved.health.phase == first.phase) ||
            (saved.sequence == 2 && saved.utc == now + day && saved.health.phase == second.phase));
        const unsigned int attempts = interrupted.writes;
        interrupted.failAfter = -1;
        assert(writer.save(second, now + 2 * day, true) == SaveResult::StorageError);
        assert(interrupted.writes == attempts); // No endless flash retry/erase loop.
    }

    FakeStorage damaged;
    damaged.bytes[0][0] = 1;
    Journal unknown(damaged);
    assert(unknown.save(first, now, true) == SaveResult::Corrupt && damaged.writes == 0);
    FakeStorage erased;
    std::memset(erased.bytes, 0xff, sizeof(erased.bytes));
    Journal fresh(erased);
    assert(fresh.save(first, now, true) == SaveResult::Saved);
    Journal tooFrequent(erased, 60);
    assert(tooFrequent.save(first, now + day, true) == SaveResult::InvalidInput);

    FakeStorage bounded;
    Journal limited(bounded);
    for (uint32_t i = 0; i < MAX_SAVED_CHECKPOINTS; ++i) {
        assert(limited.save(first, now + i * day, true) == SaveResult::Saved);
    }
    assert(limited.save(first, now + MAX_SAVED_CHECKPOINTS * day, true) == SaveResult::Exhausted);
    assert(bounded.writes == MAX_SAVED_CHECKPOINTS);

    FakeStorage conflicting;
    Journal ambiguous(conflicting);
    assert(ambiguous.save(first, now, true) == SaveResult::Saved);
    assert(ambiguous.load(saved));
    saved.health.phase = Phase::Boot;
    saved.checksum = Journal::checksum(saved);
    assert(conflicting.write(1, saved)); // Same sequence, different valid contents: no winner.
    assert(!ambiguous.load(saved));
    assert(ambiguous.save(first, now + day, true) == SaveResult::Corrupt);
    assert(conflicting.writes == 2);
}
