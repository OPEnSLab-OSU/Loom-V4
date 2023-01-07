#pragma once

#include <Wire.h>

#include "../I2CDevice.h"
#include "Loom_Manager.h"

/**
 * Functionality for the EZO Dissolved Oxygen Sensor
 * 
 * @author Will Richards
 */  
class Loom_EZODO : public I2CDevice{
    protected:
        void print_measurements() override {};  
        void power_up() override {}; 

    public:
        void initialize() override;
        void measure() override;
        void package() override;
        void power_down() override;

        /**
         *  Construct a new EZDO device
         *  @param man Reference to the manager
         *  @param address I2C address to communicate over
         *  @param useMux Whether or not to use the mux
         */ 
        Loom_EZODO(
                Manager& man,
                byte address            = 0x61,
                bool useMux             = false
            );


        /**
         * Get the dissolved oxygen level
         */ 
        float getOxygen() { return oxygen; };

        /**
         * Get the percent saturation
         */ 
        float getSaturation() { return saturation; };

        /* Calibrates the DO sensor for taking readings*/
        bool calibrate();
    
    private:
        Manager* manInst;                                                       // Instance of the manager
        
        float oxygen;                                                           // Reading for the DO value
        float saturation;                                                       // Get the percent saturation

        bool sendTransmission(String command);                                  // Send a specified command to the device
        void parseResponse();                                                   // Parse the returned string into the appropriate numeric variables

        byte i2c_address;
        int8_t code = 0;                                                        // I2C Response Code
        char currentChar;                                                       // Current character we have read in
        String responseCodes[4] = {"Success", "Failed", "Pending", "No Data"};  // Stringified I2C Response codes
        int waitTime = 575;                                                     // The length of time we should wait before collecting data
        String sensorData;                                                      // Convert the char array to a string to improve parse-ability

       
};