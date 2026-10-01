#pragma once
#include "Arduino.h"

class FakeWatchdog {
  public:
    unsigned int feeds = 0;
    unsigned int enables = 0;
    void disable() { WDT->CTRL.bit.ENABLE = 0; }
    void reset() { ++feeds; }
    int enable(int milliseconds) {
        ++enables;
        WDT->CTRL.bit.ENABLE = 1;
        WDT->CONFIG.bit.PER = 0; // Detect accidental substitution of the default configuration.
        WDT->CTRL.bit.WEN = 0;
        return milliseconds;
    }
};
extern FakeWatchdog Watchdog;
