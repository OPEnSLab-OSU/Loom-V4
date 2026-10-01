#pragma once
#include "Loom_WarningGuards.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <Arduino.h>
LOOM_EXTERNAL_INCLUDE_END

////////////////////////////////////////////////////////////////////////////////////////////////////
// Optional SAMD21 supply warning. This monitors the MCU's 3.3 V supply, NOT battery voltage.
// The sketch owns SYSCTRL_Handler and forwards it to handleInterrupt(). No vector is replaced
// merely by including Loom. Only use after calibrating an early-warning LEVEL on your stack.
class Loom_BrownoutGuard {
  public:
    Loom_BrownoutGuard() = default;
    ~Loom_BrownoutGuard() { end(); }
    Loom_BrownoutGuard(const Loom_BrownoutGuard &) = delete;
    Loom_BrownoutGuard &operator=(const Loom_BrownoutGuard &) = delete;
    bool begin(uint8_t warningLevel); // Requires an existing, lower RESET threshold.
    void end();                       // Restore the original hardware reset configuration.
    void handleInterrupt();           // ISR: latch one flag and restore the reset safeguard.
    bool takeSignal();                // Loop: consume the flag before starting more work.
    bool isArmed() const { return armed; }

  private:
    bool restoreReset();
    uint32_t resetConfiguration = 0;
    volatile bool pending = false;
    volatile bool armed = false;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
