#include <cassert>
#include "Utilities/Loom_HeartbeatSchedule.h"
int main() {
    using Event = loomHeartbeat::Schedule::Event;
    loomHeartbeat::Schedule timers(3600, 900);
    assert(timers.poll(1000) == Event::None);
    assert(timers.begin(1000));
    assert(timers.poll(1000) == Event::Work);
    assert(timers.poll(1000) == Event::None && timers.secondsUntilNext(1000) == 900);
    assert(timers.poll(1900) == Event::Work);
    assert(timers.poll(4600) == Event::Work); // Skip missed work; retain due heartbeat.
    assert(timers.secondsUntilNext(4600) == 0);
    assert(timers.poll(4600) == Event::Heartbeat);
    assert(timers.secondsUntilNext(4600) == 900);
    assert(timers.poll(900) == Event::Work); // Backward clock starts new deadlines explicitly.
    assert(timers.secondsUntilNext(900) == 900);
    loomHeartbeat::Schedule invalid(0, 900);
    assert(!invalid.begin(1000));
    assert(!timers.begin(UINT32_MAX - 1));
    assert(timers.poll(UINT32_MAX) == Event::None);
}
