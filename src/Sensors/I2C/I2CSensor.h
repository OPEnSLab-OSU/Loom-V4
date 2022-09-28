#pragma once

#include "Module.h"
#include <Wire.h>

/**
 * Wrapper to manage I2C sensors
 */ 
class I2CSensor : public Module{
    public:
        I2CSensor(String moduleName) : Module(moduleName)  {};

        /* Checks if the given I2C device is currently connected*/
        bool checkDeviceConnection() {
            Wire.beginTransmission(module_address);
            return Wire.endTransmission() == 0;
        };
};