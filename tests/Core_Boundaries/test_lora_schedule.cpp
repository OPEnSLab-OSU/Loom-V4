#include <cassert>
#include "Utilities/Loom_LoRaSchedule.h"
int main() {
    loomLoRa::Schedule schedule;
    assert(schedule.mayTransmit(255, 1)); // Disabled preserves ordinary send behavior.
    assert(schedule.secondsUntilWake(255, 1) == 0);
    assert(schedule.configure(300, 12));
    assert(loomLoRa::Schedule::group(0x21) == 2 && loomLoRa::Schedule::device(0x21) == 1);
    assert(loomLoRa::Schedule::sameGroup(0x21, 0x20));
    assert(!loomLoRa::Schedule::sameGroup(0x21, 0x30));
    assert(!schedule.mayTransmit(0x21, 299));
    assert(schedule.mayTransmit(0x21, 300) && schedule.mayTransmit(0x21, 599));
    assert(!schedule.mayTransmit(0x21, 600));
    assert(schedule.secondsLeftInSlot(0x21, 599) == 1);
    assert(schedule.secondsUntilWake(0x21, 239) == 1);
    assert(schedule.secondsUntilWake(0x21, 240) == 0);
    assert(schedule.secondsUntilWake(0x21, 599) == 0);
    assert(schedule.secondsUntilWake(0x21, 600) == 3240);
    assert(schedule.secondsUntilWake(0x20, 3540) == 0); // Slot zero prepares in prior cycle.
    assert(schedule.secondsUntilWake(0x2f, 0) == UINT32_MAX);
    assert(!schedule.configure(300, 16)); // Does not fit one hour; old settings survive.
    assert(schedule.mayTransmit(0x21, 300));
    assert(!schedule.configure(0, 1) && !schedule.configure(1, 0));
    assert(!schedule.configure(1, 17) && !schedule.configure(1, 1, 0));
    assert(!schedule.configure(1, 1, 3600, 3600));
    assert(schedule.configure(225, 16));
    assert(schedule.mayTransmit(0x2f, 3375));
    assert(schedule.cycleNumber(7200) == 2);
    schedule.disable();
    assert(schedule.mayTransmit(0x2f, 0));
}
