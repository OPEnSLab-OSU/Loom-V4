#pragma once

// Stub for OPEnS_RTC.h so it compiles on desktop
class OPEnS_RTC {
public:
    void begin() {}
    unsigned long now() { return 0; }
};