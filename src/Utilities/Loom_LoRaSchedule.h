#pragma once
#include <cstdint>

namespace loomLoRa {
////////////////////////////////////////////////////////////////////////////////////////////////////
// One UTC cycle contains one window per device. Address bits select group and window.
// No clock is owned here: pass RTC UTC seconds, so nodes share a phase across sleep/restarts.
class Schedule {
  public:
    bool configure(uint32_t windowSeconds, uint8_t devices, uint32_t cycleSeconds = 3600,
                   uint32_t earlyWakeSeconds = 60) {
        if (windowSeconds == 0 || devices == 0 || devices > 16 || cycleSeconds == 0 ||
            cycleSeconds > 27UL * 86400UL || windowSeconds > cycleSeconds / devices ||
            earlyWakeSeconds >= cycleSeconds || earlyWakeSeconds > cycleSeconds - windowSeconds) {
            return false;
        }
        window = windowSeconds;
        count = devices;
        cycle = cycleSeconds;
        early = earlyWakeSeconds;
        enabled = true;
        return true;
    }
    void disable() { enabled = false; }
    bool isEnabled() const { return enabled; }
    static uint8_t group(uint8_t address) { return address >> 4; }
    static uint8_t device(uint8_t address) { return address & 15; }
    static bool sameGroup(uint8_t first, uint8_t second) { return group(first) == group(second); }
    uint32_t cycleNumber(uint32_t utc) const { return utc / cycle; }
    uint32_t secondsLeftInSlot(uint8_t address, uint32_t utc) const {
        if (!enabled) {
            return UINT32_MAX;
        }
        return mayTransmit(address, utc) ? window - (utc % cycle - device(address) * window) : 0;
    }

    bool mayTransmit(uint8_t address, uint32_t utc) const {
        if (!enabled) {
            return true;
        }
        const uint8_t slot = device(address);
        const uint32_t phase = utc % cycle;
        return slot < count && phase >= slot * window && phase - slot * window < window;
    }
    // Time until the next pre-measurement wake. Zero means this window's preparation/send
    // period is already open. UINT32_MAX means this address has no configured window.
    uint32_t secondsUntilWake(uint8_t address, uint32_t utc) const {
        if (!enabled) {
            return 0;
        }
        const uint8_t slot = device(address);
        if (slot >= count) {
            return UINT32_MAX;
        }
        const uint32_t start = slot * window;
        const uint32_t wake = (start + cycle - early) % cycle;
        const uint32_t phase = utc % cycle;
        const uint32_t sinceWake = (phase + cycle - wake) % cycle;
        if (sinceWake < early + window) {
            return 0;
        }
        return (wake + cycle - phase) % cycle;
    }

  private:
    uint32_t window = 300, cycle = 3600, early = 60;
    uint8_t count = 12;
    bool enabled = false;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace loomLoRa
