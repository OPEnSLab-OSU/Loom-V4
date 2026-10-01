// Written for the next authorized compiler run. Build with Core_Boundaries/fakes on the include
// path. These register fakes verify state ownership, not oscillator timing or board sleep.
#include "../../src/Utilities/Loom_Watchdog.h"
#include <assert.h>

FakeWatchdogRegisters registers;
FakeWatchdogRegisters *WDT = &registers;
FakeWatchdog Watchdog;

void leaveEarly() {
    LoomWatchdogPause pause;
    assert(!loomWatchdogIsEnabled());
    return;
}

int main() {
    {
        LoomWatchdogPause initiallyDisabled;
        assert(!loomWatchdogIsEnabled());
    }
    assert(!loomWatchdogIsEnabled());
    assert(Watchdog.feeds == 0);

    registers.CTRL.bit.ENABLE = 1;
    registers.CTRL.bit.WEN = 1;
    registers.CONFIG.bit.PER = 9;
    registers.CONFIG.bit.WINDOW = 4;
    {
        LoomWatchdogPause outer;
        assert(!loomWatchdogIsEnabled());
        {
            LoomWatchdogPause inner;
            assert(!loomWatchdogIsEnabled());
        }
        assert(!loomWatchdogIsEnabled()); // Inner destructor must not restore outer's timer.
    }
    assert(loomWatchdogIsEnabled());
    assert(registers.CTRL.bit.WEN == 1);
    assert(registers.CONFIG.bit.PER == 9);
    assert(registers.CONFIG.bit.WINDOW == 4);
    assert(Watchdog.feeds == 1);
    assert(Watchdog.enables == 0); // Restore retained registers, never enable(defaultTimeout).

    leaveEarly();
    assert(loomWatchdogIsEnabled());
    assert(registers.CONFIG.bit.PER == 9);
    assert(Watchdog.feeds == 2);
    loomResetWatchdogIfEnabled();
    assert(Watchdog.feeds == 3);
    registers.CTRL.bit.ENABLE = 0;
    loomResetWatchdogIfEnabled();
    assert(Watchdog.feeds == 3);
    return 0;
}
