#pragma once
#include "Loom_TimeUtils.h"

namespace loomHeartbeat {
////////////////////////////////////////////////////////////////////////////////////////////////////
// Two deadlines on the same UTC clock. No alarm, modem or measurement packet is owned here.
class Schedule {
  public:
    enum class Event { None, Work, Heartbeat };
    Schedule(uint32_t heartbeatSeconds, uint32_t workSeconds)
        : heartbeatInterval(heartbeatSeconds), workInterval(workSeconds) {}
    bool begin(uint32_t nowUtc) {
        if (heartbeatInterval > INT32_MAX || workInterval > INT32_MAX ||
            !loomTime::validRtcWakeDelay(static_cast<int32_t>(heartbeatInterval)) ||
            !loomTime::validRtcWakeDelay(static_cast<int32_t>(workInterval)) ||
            !loomTime::nextSampleTime(nowUtc, heartbeatInterval, 0, nextHeartbeat)) {
            started = false;
            return false;
        }
        nextWork = nowUtc; // First normal measurement is due immediately.
        lastUtc = nowUtc;
        started = true;
        return true;
    }
    Event poll(uint32_t nowUtc) {
        if (!started) {
            return Event::None;
        }
        // A corrected clock can move backward. Start a fresh schedule from that clock;
        // otherwise the old deadlines could leave the device waiting for hours.
        if (nowUtc < lastUtc && !begin(nowUtc)) {
            return Event::None;
        }
        lastUtc = nowUtc;
        // Work wins a tie. Poll again after work so a due heartbeat is still delivered.
        if (nowUtc >= nextWork) {
            started = loomTime::nextSampleTime(nowUtc, workInterval, nextWork, nextWork);
            return Event::Work;
        }
        if (nowUtc >= nextHeartbeat) {
            started =
                loomTime::nextSampleTime(nowUtc, heartbeatInterval, nextHeartbeat, nextHeartbeat);
            return Event::Heartbeat;
        }
        return Event::None;
    }
    uint32_t secondsUntilNext(uint32_t nowUtc) const {
        if (!started || nowUtc < lastUtc || nowUtc >= nextWork || nowUtc >= nextHeartbeat) {
            return 0;
        }
        return (nextWork < nextHeartbeat ? nextWork : nextHeartbeat) - nowUtc;
    }

  private:
    uint32_t heartbeatInterval, workInterval;
    uint32_t nextHeartbeat = 0, nextWork = 0, lastUtc = 0;
    bool started = false;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace loomHeartbeat
