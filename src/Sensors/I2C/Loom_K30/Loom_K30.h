#pragma once

#include <Wire.h>
#include "wiring_private.h"

#include "Loom_Manager.h"
#include "../I2CDevice.h"


/**
 * Handles communication with the K30 sensor
 * 
 * Product Page: https://www.co2meter.com/products/k-30-co2-sensor-module?variant=8463942
 * @author Will Richards
 */ 
class Loom_K30 : public I2CDevice{
    protected:
        void power_up() override {};
        void power_down() override {}; 
        
       
        // Manager controlled functions
        void measure() override;                               
        void initialize() override;    
        void package() override;
    public:
        /**
         * Construct a new K30 sensor using I2C to communicate
         * @param man Reference to the manager
         * @param useMux If this module will be using the mux
         * @param addr I2C address
         * @param warmUp If we should wait 6 minutes before collecting data to allow the sensor to warm up
         * @param valMult How much to multiply the recorded output by
         */ 
        Loom_K30(
            Manager& man,
            bool useMux = false, 
            int addr = 0x68,  
            bool warmUp = true, 
            int valMult = 1
        );
        
        /**
         * Get the current CO2 levels collected by the sensor
         */ 
        float getCO2() { return (float)CO2Levels; };

    private:
        Manager* manInst;                                               // Instance of the manager

        int addr;                                                       // Address of the I2C sensor

        int CO2Levels;                                                  // Current CO2 levels of the sensor

        int valMult;                                                    // Return value multiplier
        bool warmUp;                                                    // Should we wait 6 mins for warm up to get accurate results
        bool hasWarmedUp = false;

        byte buffer[4] = {0, 0, 0, 0};                                  // Buffer to store data read from the sensor

        byte read_CO2[7] = {0xFE, 0X44, 0X00, 0X08, 0X02, 0X9F, 0X25};  // Command packet to read Co2 (see app note)
        byte sensor_response[7] = {0, 0, 0, 0, 0, 0, 0};                // create an array to store the response

        bool verifyChecksum();                                          // Verify the checksum for the data is correct
        void getCO2Level();                                             // Request the CO2 Level from the sensor
};