#pragma once
#include <stdint.h>

class TimeSpan {
  public:
    explicit TimeSpan(int32_t value) : seconds(value) {}
    int32_t totalseconds() const { return seconds; }

  private:
    int32_t seconds;
};

// Model observable alarm/clock outcomes only, without pretending to emulate an RTC.
class Loom_Hypnos {
  public:
    bool alarmReady = true;
    bool clockReady = false;
    int scheduleCalls = 0;
    int32_t lastInterval = 0;
    bool scheduleWake(TimeSpan duration) {
        ++scheduleCalls;
        lastInterval = duration.totalseconds();
        return alarmReady;
    }
    bool networkTimeUpdate() { return clockReady; }
};
