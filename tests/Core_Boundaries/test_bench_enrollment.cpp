#include <cassert>
#include "Radio/Loom_LoRa/Benchmark/BenchEnrollment.h"

int main() {
    using namespace bench;
    const BoardKey north = boardFingerprint("00112233445566778899AABBCCDDEEFF");
    const BoardKey south = boardFingerprint("00112233445566778899AABBCCDDEEFE");
    const BoardKey spare = boardFingerprint("112233445566778899AABBCCDDEEFF00");
    assert(north && south && spare && north != south && north != spare && south != spare);
    assert(boardFingerprint(nullptr) == 0 && boardFingerprint("") == 0);

    uint8_t wire[BoardKeyBytes];
    writeBoardKey(wire, 0x0123456789abcdefULL);
    assert(wire[0] == 0xef && wire[7] == 0x01);
    assert(readBoardKey(wire) == 0x0123456789abcdefULL);

    EnrollmentSlots<16> hub;
    assert(hub.assign(north, 1, 2) == 1);
    assert(hub.assign(south, 1, 2) == 2); // Both requested device number 1.
    assert(hub.assign(spare, 1, 2) == 0); // Don't displace a disconnected board.
    assert(hub.assign(south, 1, 2) == 2); // Same physical board, even after a power cycle.
    assert(hub.assign(north, 2, 2) == 1); // A changed preference doesn't move a lease.
    assert(hub.assign(south, 1, 1) == 0 && hub.highestReserved() == 2);
    assert(!hub.forget(0) && !hub.forget(17) && hub.forget(2));
    assert(hub.highestReserved() == 1 && hub.assign(spare, 16, 2) == 2);
    assert(hub.keyAt(2) == spare && hub.keyAt(2) != south); // Stale slot fails the roster check.
    assert(hub.assign(south, 2, 3) == 3);

    EnrollmentSlots<20> fullArray;
    for (uint8_t slot = 1; slot < 20; ++slot) {
        assert(fullArray.assign(slot, slot, 20) == slot);
    }
    assert(fullArray.assign(north, 1, 20) == 20);
    assert(fullArray.highestReserved() == 20 && fullArray.assign(north, 1, 20) == 20);
    assert(fullArray.assign(south, 20, 20) == 0);

    bool separate = false;
    for (uint16_t window = 1; window <= 32; ++window) {
        assert(joinBucket(north, window) < JoinSlots);
        assert(joinBucket(south, window) < JoinSlots);
        separate |= joinBucket(north, window) != joinBucket(south, window);
    }
    assert(separate); // A shared contention slot needn't repeat every window.
}
