#pragma once

#include "Module.h"

class I2CDevice : public Module{
    public:

        /* Construct a new I2C device */
        I2CDevice(String modName) : Module(modName) {};

        /* Checks if the given I2C device is currently connected*/
        bool checkDeviceConnection() {
            if(module_address != -1){
                Wire.beginTransmission(module_address);
                if(Wire.endTransmission() == 0){
                    return true;
                }
                else{
                    needsReinit = true;
                    return false;
                }
            }
            return false;
        };

        bool needsReinit = false;                      // Whether or not the device needs to be reinitialized
};