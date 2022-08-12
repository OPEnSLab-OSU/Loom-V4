#pragma once

#include <Wire.h>

#include "Module.h"
#include "Loom_Manager.h"

#define RangeCommand    		0x51	///< The Sensor ranging command has a value of 0x51
#define ChangeAddressCommand1 	0xAA	///< These are the two commands that need to be sent in sequence to change the sensor address
#define ChangeAddressCommand2 	0xA5	///< These are the two commands that need to be sent in sequence to change the sensor address

/**
 * MB1232 Distance sensor
 * 
 * @author Will Richards
 */ 
class Loom_MB1232 : public Module{
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
         * Constructs a new TSL2591 sensor
         * @param man Reference to the manager that is used to universally package all data
         * @param address I2C address that is assigned to the sensor
         */ 
        Loom_MB1232(
                    Manager& man, 
                    int addr = 0x70
                );

        /**
         * Get the measured distance
         */ 
        uint16_t getRange() { return range; };



    private:
        Manager* manInst;                       // Instance of the manager
        int address;

        uint16_t range;                         // Measure distance in cm
};