#pragma once

#include "Loom_WarningGuards.h"
#include "Module.h"
#include "Logger.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <Wire.h>
LOOM_EXTERNAL_INCLUDE_END

class I2CDevice : public Module{
    public:

        /* Construct a new I2C device */
        I2CDevice(const char* modName) : Module(modName) {};

        /* Checks if the given I2C device is currently connected*/
        bool checkDeviceConnection() {
            FUNCTION_START;
            if(module_address != -1){
                Wire.beginTransmission(module_address);
                if(Wire.endTransmission() == 0){
                    FUNCTION_END;
                    return true;
                }
                else{
                    needsReinit = true;
                    FUNCTION_END;
                    return false;
                }
            }
            FUNCTION_END;
            return false;
        };


        bool needsReinit = false;                      // Whether or not the device needs to be reinitialized
};
