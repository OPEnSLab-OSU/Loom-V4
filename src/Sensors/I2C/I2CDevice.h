#pragma once

#include "Loom_WarningGuards.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <Wire.h>
LOOM_EXTERNAL_INCLUDE_END

#include "Logger.h"
#include "Module.h"

class I2CDevice : public Module {
  public:
    /* Construct a new I2C device */
    I2CDevice(const char *modName) : Module(modName) {};

    /* Checks if the given I2C device is currently connected*/
    bool checkDeviceConnection() {
        FUNCTION_START(this);
        if (module_address < 0 || module_address > 0x7F) {
            return false;
        }
        // Reject an unset/out-of-range 7-bit address before narrowing it for Wire.
        Wire.beginTransmission(static_cast<uint8_t>(module_address));
        if (Wire.endTransmission() == 0) {
            return true;
        }
        needsReinit = true;
        FUNCTION_END;
        return false;
    };

    bool needsReinit = false; // Whether or not the device needs to be reinitialized
};
