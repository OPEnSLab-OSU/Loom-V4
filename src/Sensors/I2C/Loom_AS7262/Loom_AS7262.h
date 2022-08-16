#pragma once

#include <AS726X.h>
#include <Wire.h>

#include "Module.h"
#include "Loom_Manager.h"

/**
 * AS7262 Color sensor
 * 
 * @author Will Richards
 */ 
class Loom_AS7262 : public Module{
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
         * @param gain Gain level
         * @param mode Read Mode: 0("4 channels out of 6"), 1("Different 4 channels out of 6"), 2("All 6 channels continuously"), 3("One-shot reading of all channels") 
         * @param integration_time Integration time (time will be 2.8ms * [integration value])
         */ 
        Loom_AS7262(
                    Manager& man, 
                    int addr = 0x49,
                    uint8_t gain = 1,
                    uint8_t mode = 3,
                    uint8_t integration_time = 50
                );
    private:
        Manager* manInst;                       // Instance of the manager
        AS726X asInst;                          // Instance of the AS7262
        
        uint16_t	color[6];			        // Measured color bands values. Units: counts / (μW/cm^2)

        uint8_t		gain;				        // Gain setting
        uint8_t		mode;				        // Sensor mode
        uint8_t		integration_time;	        // Integration time setting
};