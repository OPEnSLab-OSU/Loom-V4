#pragma once
#include <stdint.h>

namespace loomTime {
// SdFat can request several timestamps while opening/syncing one record. Share a checked
// RTC observation for less than one second; FAT modified times have two-second precision.
// Do not extrapolate across standby: invalidate before sleep and refresh after waking.
class FileTimestampCache {
  public:
    bool fresh(uint32_t nowMs) const {
        return checked && static_cast<uint32_t>(nowMs - checkedMs) < 1000UL;
    }
    bool get(uint32_t nowMs, uint32_t &utc) const {
        if (!fresh(nowMs) || !valid) {
            return false;
        }
        utc = utcSeconds;
        return true;
    }
    void update(uint32_t utc, uint32_t nowMs) {
        utcSeconds = utc;
        checkedMs = nowMs;
        checked = valid = true;
    }
    void unavailable(uint32_t nowMs) {
        checkedMs = nowMs;
        checked = true;
        valid = false;
    }
    void invalidate() { checked = valid = false; }

  private:
    uint32_t utcSeconds = 0;
    uint32_t checkedMs = 0;
    bool checked = false;
    bool valid = false;
};
} // namespace loomTime
