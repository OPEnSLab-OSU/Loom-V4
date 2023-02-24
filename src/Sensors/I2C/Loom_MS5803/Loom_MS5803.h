#pragma once

#include "../I2CDevice.h"
#include "Loom_Manager.h"
#include <Wire.h>

#include <MS5803_02.h>

/**
 * Interface for the MS5803 Atmospheric Pressure / Temperature sensor module.
 * 
 * @author Will Richards
 */ 

class Loom_MS5803 : public I2CDevice{
    protected:
        
        void power_down() override {};
         
    public:

        /**
         * Module control methods
         */ 
        void initialize() override;
        void measure() override;
        void package() override;

        void power_up() override;

        Loom_MS5803(Manager& man, byte address = 0x77, bool useMux = false);

         /**
         * Get the temperature reading
         */ 
        float getTemperature() { return sensorData[0]; };

        /**
         * Get the pressure reading
         */ 
        float getPressure() { return sensorData[1]; };

    private:

        Manager* manInst;       // Instance of the manager
        MS_5803 inst;           // Instance of the MS5803 

        float sensorData[2];    // Stores the temperature and pressure collected by the sensor

        bool initialized = true; // True until set to false
};