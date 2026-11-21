#pragma once

#include "Loom_WarningGuards.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <RH_RF95.h>
LOOM_EXTERNAL_INCLUDE_END

// RadioHead waits for DIO0 to leave TX or CAD. Bound both waits so a radio
// brownout or missing interrupt cannot strand a node between samples.
class Loom_RF95 : public RH_RF95 {
  public:
    using RH_RF95::RH_RF95;
    // Loom uses SF7 at 125 kHz or faster. This also covers a full-length frame.
    static constexpr uint16_t InterruptTimeoutMs = 1000;

    bool waitPacketSent() override { return waitPacketSent(InterruptTimeoutMs); }

    bool waitPacketSent(uint16_t timeout) override {
        if (RH_RF95::waitPacketSent(timeout < InterruptTimeoutMs ? timeout : InterruptTimeoutMs)) {
            return true;
        }
        setModeIdle();
        return false;
    }

    bool isChannelActive() override {
        if (_mode != RHModeCad) {
            spiWrite(RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_CAD);
            spiWrite(RH_RF95_REG_40_DIO_MAPPING1, 0x80);
            _mode = RHModeCad;
        }
        const uint32_t started = millis();
        while (_mode == RHModeCad) {
            if (millis() - started >= InterruptTimeoutMs) {
                setModeIdle();
                return true;
            }
            YIELD;
        }
        return _cad;
    }
};
