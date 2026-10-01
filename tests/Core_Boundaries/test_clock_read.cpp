// Source-only until compilers resume. Exercise the production boundary, not an RTC simulator.
#include <cassert>
#include "Utilities/Loom_ClockRead.h"

uint32_t testMillis = 0;
uint32_t readMillis() { return testMillis; }
struct DateFields {
    int y = 2026, m = 9, d = 30, h = 0, n = 0, s = 0;
    int year() const { return y; }
    int month() const { return m; }
    int day() const { return d; }
    int hour() const { return h; }
    int minute() const { return n; }
    int second() const { return s; }
    uint32_t unixtime() const { return loomTime::unixSeconds(y, m, d, h, n, s); }
};
struct ClockFake {
    bool stopped = false, statusOK = true, readOK = true, writeOK = true;
    bool forceObserved = false, leaveStopped = false, operationOK = true;
    unsigned int reads = 0, writes = 0;
    uint32_t readDelayMs = 0;
    DateFields value;
    bool lostPower() {
        operationOK = statusOK;
        return stopped;
    }
    bool lastOperationSucceeded() const { return operationOK; }
    DateFields now() {
        ++reads;
        testMillis += readDelayMs;
        operationOK = readOK;
        return value;
    }
    bool adjustChecked(const DateFields &requested) {
        ++writes;
        operationOK = writeOK && requested.unixtime() != 0;
        if (!operationOK) {
            return false;
        }
        stopped = leaveStopped;
        if (!forceObserved) {
            value = requested;
        }
        return true;
    }
};

int main() {
    DateFields output;
    output.y = 2025;
    const uint32_t original = output.unixtime();
    ClockFake clock;
    clock.stopped = true;
    assert(!loomTime::readEstablishedUtc(clock, output));
    assert(output.unixtime() == original && clock.reads == 0);
    clock.stopped = false;
    clock.statusOK = false;
    assert(!loomTime::readEstablishedUtc(clock, output) && clock.reads == 0);
    clock.statusOK = true;
    clock.readOK = false;
    assert(!loomTime::readEstablishedUtc(clock, output) && output.unixtime() == original);
    clock.readOK = true;
    clock.value.m = 13;
    assert(!loomTime::readEstablishedUtc(clock, output) && output.unixtime() == original);
    clock.value.m = 9;
    assert(loomTime::readEstablishedUtc(clock, output) && output.y == 2026);

    DateFields requested;
    clock = ClockFake{};
    assert(!loomTime::writeVerifiedUtc(clock, requested, nullptr) && clock.writes == 0);
    clock.writeOK = false;
    assert(!loomTime::writeVerifiedUtc(clock, requested, readMillis) && clock.reads == 0);
    clock.writeOK = true;
    clock.leaveStopped = true;
    assert(!loomTime::writeVerifiedUtc(clock, requested, readMillis));
    clock.leaveStopped = false;
    clock.readOK = false;
    assert(!loomTime::writeVerifiedUtc(clock, requested, readMillis));
    clock.readOK = true;
    assert(loomTime::writeVerifiedUtc(clock, requested, readMillis));
    clock.forceObserved = true;
    clock.value.s = 1;
    assert(loomTime::writeVerifiedUtc(clock, requested, readMillis)); // One second tick allowed.
    clock.value.s = 3;
    assert(!loomTime::writeVerifiedUtc(clock, requested, readMillis)); // ACK with wrong date.
    clock.readDelayMs = 2000;
    testMillis = UINT32_MAX - 999;
    assert(loomTime::writeVerifiedUtc(clock, requested, readMillis)); // Delay across timer wrap.
    clock.readDelayMs = 0;
    clock.value.y = 2025;
    assert(!loomTime::writeVerifiedUtc(clock, requested, readMillis)); // Old clock not updated.
}
