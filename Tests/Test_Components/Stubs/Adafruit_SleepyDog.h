#pragma once

class Watchdog {
public:
    void begin(unsigned long) {}
    void reset() {}
    void end() {}
};

extern Watchdog Watchdog;