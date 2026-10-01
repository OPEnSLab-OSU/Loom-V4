#pragma once

#include "Utilities/Loom_Watchdog.h"

namespace loomReset {
inline uint8_t bootCause() {
#if defined(ARDUINO_ARCH_SAMD)
    return Watchdog.resetCause();
#else
    return 0; // This diagnostic targets SAMD hardware; zero means unknown, not power loss.
#endif
}

inline bool softwareResetOnly(uint8_t cause) {
#if defined(ARDUINO_ARCH_SAMD)
#if defined(__SAMD51__)
    return cause == RSTC_RCAUSE_SYST;
#else
    return cause == PM_RCAUSE_SYST;
#endif
#else
    (void)cause;
    return false;
#endif
}

// Preserve all reset bits in the SD record. A short label is only a reading aid: brownout,
// watchdog, reset button, and software reset must not all be called "lost power".
inline const char *causeName(uint8_t cause) {
#if defined(ARDUINO_ARCH_SAMD)
#if defined(__SAMD51__)
    switch (cause) {
    case RSTC_RCAUSE_POR:
        return "power-on";
    case RSTC_RCAUSE_BODCORE:
        return "core-brownout";
    case RSTC_RCAUSE_BODVDD:
        return "supply-brownout";
    case RSTC_RCAUSE_EXT:
        return "external-reset";
    case RSTC_RCAUSE_WDT:
        return "watchdog";
    case RSTC_RCAUSE_SYST:
        return "software-reset";
    default:
        return "unknown-or-multiple";
    }
#else
    switch (cause) {
    case PM_RCAUSE_POR:
        return "power-on";
    case PM_RCAUSE_BOD12:
        return "core-brownout";
    case PM_RCAUSE_BOD33:
        return "supply-brownout";
    case PM_RCAUSE_EXT:
        return "external-reset";
    case PM_RCAUSE_WDT:
        return "watchdog";
    case PM_RCAUSE_SYST:
        return "software-reset";
    default:
        return "unknown-or-multiple";
    }
#endif
#else
    (void)cause;
    return "unknown";
#endif
}
} // namespace loomReset
