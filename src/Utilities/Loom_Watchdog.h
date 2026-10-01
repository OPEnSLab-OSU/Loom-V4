#pragma once

#include "Loom_WarningGuards.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <Arduino.h>
#include <Adafruit_SleepyDog.h>
LOOM_EXTERNAL_INCLUDE_END

////////////////////////////////////////////////////////////////////////////////////////////////////
// Watchdog state and feeding: keep the board alive after verified progress.
////////////////////////////////////////////////////////////////////////////////////////////////////
// SleepyDog calls feeding the timer "reset". Feeding prevents a restart; it does not restart
// the Feather. These helpers also see a watchdog enabled by a sketch at runtime.
inline bool loomWatchdogIsEnabled() {
#if defined(ARDUINO_ARCH_SAMD)
#if defined(__SAMD51__)
    return WDT->CTRLA.bit.ENABLE;
#else
    return WDT->CTRL.bit.ENABLE;
#endif
#elif defined(WATCHDOG_ENABLE)
    return true;
#else
    return false;
#endif
}

inline void loomFeedWatchdogIfEnabled(const char *file, unsigned long line) {
    if (!loomWatchdogIsEnabled()) {
        return;
    }
    Watchdog.reset();
#if defined(LOOM_WATCHDOG_FEED_TRACE)
    // Opt-in Serial trace only. Writing to SD on every feed would add latency and card wear.
    // Use a project-wide build definition to trace library calls as well as sketch calls.
    Serial.print(F("[WATCHDOG FEED] "));
    Serial.print(file ? file : "callback");
    Serial.print(':');
    Serial.println(line);
#else
    (void)file;
    (void)line;
#endif
}

#define LOOM_FEED_WATCHDOG() loomFeedWatchdogIfEnabled(__FILE__, __LINE__)

// Retain the no-argument function: SD recovery passes it as a progress callback.
inline void loomResetWatchdogIfEnabled() { loomFeedWatchdogIfEnabled(nullptr, 0); }

////////////////////////////////////////////////////////////////////////////////////////////////////
// Temporary pauses: restore the caller's guard when sleep or a slow connection finishes.
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void loomResumeWatchdog(bool wasEnabled) {
    if (!wasEnabled) {
        return;
    }
#if defined(ARDUINO_ARCH_SAMD)
    // SleepyDog::disable() changes only ENABLE. Re-enable the retained configuration rather
    // than calling enable(defaultMs), which would change a sketch's timeout/window settings.
    Watchdog.reset();
#if defined(__SAMD51__)
    WDT->CTRLA.bit.ENABLE = 1;
    while (WDT->SYNCBUSY.reg) {
    }
#else
    WDT->CTRL.bit.ENABLE = 1;
    while (WDT->STATUS.bit.SYNCBUSY) {
    }
#endif
#elif defined(WATCHDOG_ENABLE)
    Watchdog.enable(10000);
#endif
}

// Pause around a bounded operation that can legitimately outlast the watchdog, such as
// cellular registration. The destructor restores the previous state on every return path.
// Nested pauses work: only the outer pause sees an enabled timer and restores it.
class LoomWatchdogPause {
  public:
    LoomWatchdogPause() : wasEnabled(loomWatchdogIsEnabled()) {
        if (wasEnabled) {
            Watchdog.disable();
        }
    }
    ~LoomWatchdogPause() { loomResumeWatchdog(wasEnabled); }
    LoomWatchdogPause(const LoomWatchdogPause &) = delete;
    LoomWatchdogPause &operator=(const LoomWatchdogPause &) = delete;

  private:
    const bool wasEnabled;
};
