#include <cassert>
#include "Utilities/Loom_LoRaJournalFormat.h"

struct JournalFile {
    static constexpr uint8_t RecordSize = 10;
    uint8_t bytes[3 * RecordSize] = {};
    uint32_t position = 0;
    bool readError = false;

    explicit operator bool() const { return true; }
    bool seekSet(uint32_t offset) {
        position = offset;
        return offset < sizeof(bytes);
    }
    int read(uint8_t *destination, uint8_t length) const {
        if (readError || length > sizeof(bytes) - position) {
            return -1;
        }
        memcpy(destination, bytes + position, length);
        return length;
    }
    bool getError() const { return readError; }
};

static void testStoredReading() {
    using namespace loomLoRa::journal;
    JournalFile file;
    constexpr uint8_t size = JournalFile::RecordSize;
    for (uint32_t i = 0; i < 3; ++i) {
        uint8_t *entry = file.bytes + i * size;
        write32(entry, 100 + i * 10);
        entry[4] = uint8_t(i + 1);
        write32(entry + size - 4, checksum(entry, size - 4));
    }
    uint8_t entry[size], expected[size];
    memcpy(expected, file.bytes, size);
    assert(findStoredReading(file, sizeof(file.bytes), size, expected, entry) ==
           StoredReading::Match);

    expected[4] ^= 1;
    write32(expected + size - 4, checksum(expected, size - 4));
    assert(findStoredReading(file, sizeof(file.bytes), size, expected, entry) ==
           StoredReading::Missing);

    write32(expected, 105);
    write32(expected + size - 4, checksum(expected, size - 4));
    assert(findStoredReading(file, sizeof(file.bytes), size, expected, entry) ==
           StoredReading::Missing);

    memcpy(expected, file.bytes, size);
    file.readError = true;
    assert(findStoredReading(file, sizeof(file.bytes), size, expected, entry) ==
           StoredReading::Error);
    file.readError = false;
    file.bytes[4] ^= 1;
    assert(findStoredReading(file, sizeof(file.bytes), size, expected, entry) ==
           StoredReading::Error);
}

int main() {
    using namespace loomLoRa::journal;
    const uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    assert(checksum(check, sizeof(check)) == 0xcbf43926UL);

    constexpr uint32_t sample = 1778016300;
    uint8_t frame[HeaderSize] = {};
    writeHeader(frame, Receipt, sample);
    assert(frame[0] == Network && frame[1] == Receipt && read32(frame + 2) == sample);

    uint8_t record[10] = {0, 0, 0, 0, 0x01, 0x09};
    write32(record, sample);
    write32(record + 6, checksum(record, 6));
    assert(read32(record) == sample);
    assert(validEntry(record, sizeof(record)));
    record[4] ^= 1;
    assert(!validEntry(record, sizeof(record)));
    testStoredReading();
}
