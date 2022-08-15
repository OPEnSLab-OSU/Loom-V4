#pragma once

#include <Wire.h>

#include "Loom_Manager.h"
#include "Module.h"


/**
 * Handles communication with the K30 sensor
 * 
 * Product Page: https://www.co2meter.com/products/k-30-co2-sensor-module?variant=8463942
 * @author Will Richards
 */ 
class Loom_K30 : public Module{
    protected:
        void power_up() override {};
        void power_down() override {}; 
        void print_measurements() override {};
       
        // Manager controlled functions
        void measure() override;                               
        void initialize() override;    
        void package() override;
    public:
        /**
         * Construct a new K30 sensor
         * @param man Reference to the manager
         * @param addr I2C address
         * @param warmUp If we should wait 6 minutes before collecting data to allow the sensor to warm up
         * @param valMult How much to multiply the recorded output by
         */ 
        Loom_K30(
            Manager& man, 
            int addr = 0x68,  
            bool warmUp = true, 
            int valMult = 1
        );

        /**
         * Get the current CO2 levels collected by the sensor
         */ 
        int getCO2() { return CO2Levels; };

    private:
        Manager* manInst;                                               // Instance of the manager
        int addr;                                                       // Address of the I2C sensor

        int CO2Levels;                                                  // Current CO2 levels of the sensor

        int valMult;                                                    // Return value multiplier
        bool warmUp;                                                    // Should we wait 6 mins for warm up to get accurate results

        byte buffer[4] = {0, 0, 0, 0};                                  // Buffer to store data read from the sensor

        bool verifyChecksum();                                          // Verify the checksum for the data is correct
};