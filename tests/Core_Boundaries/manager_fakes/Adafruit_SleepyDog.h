#pragma once
#include "Arduino.h"

class FakeWatchdog {
  public:
    unsigned int enables = 0, feeds = 0;
    int requestedMs = 0;
    void disable() { WDT->CTRL.bit.ENABLE = 0; }
    void reset() { ++feeds; }
    uint8_t resetCause() { return PM_RCAUSE_POR; }
    int enable(int milliseconds) {
        ++enables;
        requestedMs = milliseconds;
        WDT->CTRL.bit.ENABLE = 1;
        return milliseconds;
    }
};
extern FakeWatchdog Watchdog;
