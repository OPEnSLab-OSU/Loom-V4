#include "Loom_BrownoutGuard.h"

#if defined(__SAMD21G18A__)
namespace {
// Synchronization cannot wait forever, especially inside an interrupt. Failure restores a
// processor reset rather than letting code continue with an uncertain brownout safeguard.
bool waitForBrownoutSync() {
    for (uint32_t attempt = 0; attempt < 100000; ++attempt) {
        if (SYSCTRL->PCLKSR.reg & SYSCTRL_PCLKSR_B33SRDY) {
            return true;
        }
    }
    return false;
}

bool waitForBrownoutReady() {
    for (uint32_t attempt = 0; attempt < 100000; ++attempt) {
        if (SYSCTRL->PCLKSR.reg & SYSCTRL_PCLKSR_BOD33RDY) {
            return true;
        }
    }
    return false;
}
} // namespace
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_BrownoutGuard::begin(uint8_t warningLevel) {
#if defined(__SAMD21G18A__)
    if (armed || warningLevel > 63 || SYSCTRL->INTENSET.reg != 0 || !waitForBrownoutSync()) {
        return false; // This optional example cannot share an existing SYSCTRL interrupt owner.
    }
    const uint32_t original = SYSCTRL->BOD33.reg;
    const uint8_t resetLevel =
        static_cast<uint8_t>((original & SYSCTRL_BOD33_LEVEL_Msk) >> SYSCTRL_BOD33_LEVEL_Pos);
    if (!(original & SYSCTRL_BOD33_ENABLE) ||
        (original & SYSCTRL_BOD33_ACTION_Msk) != SYSCTRL_BOD33_ACTION_RESET ||
        (original & SYSCTRL_BOD33_MODE) || warningLevel <= resetLevel) {
        // Only continuous RESET baselines are supported; leave sampled detector policy alone.
        return false;
    }
    resetConfiguration = original;
    SYSCTRL->BOD33.reg = original & ~SYSCTRL_BOD33_ENABLE;
    if (!waitForBrownoutSync()) {
        NVIC_SystemReset();
        return false;
    }
    // Microchip requires LEVEL changes while disabled. Continuous detection avoids a sampled
    // warning being missed; RUNSTDBY keeps the optional warning active during processor sleep.
    uint32_t warning = original & ~(SYSCTRL_BOD33_ENABLE | SYSCTRL_BOD33_LEVEL_Msk |
                                    SYSCTRL_BOD33_ACTION_Msk | SYSCTRL_BOD33_MODE);
    warning |= SYSCTRL_BOD33_LEVEL(warningLevel) | SYSCTRL_BOD33_ACTION_INTERRUPT |
               SYSCTRL_BOD33_HYST | SYSCTRL_BOD33_RUNSTDBY;
    SYSCTRL->BOD33.reg = warning;
    if (!waitForBrownoutSync()) {
        NVIC_SystemReset();
        return false;
    }
    SYSCTRL->INTFLAG.reg = SYSCTRL_INTFLAG_BOD33DET;
    pending = false;
    armed = true;
    SYSCTRL->INTENSET.reg = SYSCTRL_INTENSET_BOD33DET;
    SYSCTRL->BOD33.reg = warning | SYSCTRL_BOD33_ENABLE;
    NVIC_ClearPendingIRQ(SYSCTRL_IRQn);
    NVIC_EnableIRQ(SYSCTRL_IRQn);
    if (!waitForBrownoutSync() || !waitForBrownoutReady()) {
        NVIC_SystemReset();
        return false;
    }
    return true;
#else
    (void)warningLevel;
    return false;
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_BrownoutGuard::restoreReset() {
#if defined(__SAMD21G18A__)
    SYSCTRL->INTENCLR.reg = SYSCTRL_INTENCLR_BOD33DET;
    if (!waitForBrownoutSync()) {
        return false; // A write would be ignored while synchronization is busy.
    }
    SYSCTRL->BOD33.reg &= ~SYSCTRL_BOD33_ENABLE;
    if (!waitForBrownoutSync()) {
        return false;
    }
    SYSCTRL->BOD33.reg = resetConfiguration & ~SYSCTRL_BOD33_ENABLE;
    if (!waitForBrownoutSync()) {
        return false;
    }
    SYSCTRL->BOD33.reg = resetConfiguration;
    return waitForBrownoutSync() && waitForBrownoutReady();
#else
    return false;
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_BrownoutGuard::handleInterrupt() {
#if defined(__SAMD21G18A__)
    if (armed && (SYSCTRL->INTFLAG.reg & SYSCTRL_INTFLAG_BOD33DET)) {
        pending = true;
        armed = false;
        if (!restoreReset()) {
            NVIC_SystemReset();
        }
        SYSCTRL->INTFLAG.reg = SYSCTRL_INTFLAG_BOD33DET;
        NVIC_DisableIRQ(SYSCTRL_IRQn); // One warning per arm; no repeated low-supply ISR loop.
    }
#endif
    // No SD, modem, ADC, heap, Serial or delay calls in this interrupt.
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_BrownoutGuard::end() {
#if defined(__SAMD21G18A__)
    if (armed) {
        const uint32_t interruptMask = __get_PRIMASK();
        __disable_irq();
        armed = false;
        if (!restoreReset()) {
            NVIC_SystemReset();
        }
        SYSCTRL->INTFLAG.reg = SYSCTRL_INTFLAG_BOD33DET;
        NVIC_DisableIRQ(SYSCTRL_IRQn);
        __set_PRIMASK(interruptMask);
    }
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_BrownoutGuard::takeSignal() {
#if defined(__SAMD21G18A__)
    const uint32_t interruptMask = __get_PRIMASK();
    __disable_irq();
    const bool signaled = pending;
    pending = false;
    __set_PRIMASK(interruptMask);
    return signaled;
#else
    return false;
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
