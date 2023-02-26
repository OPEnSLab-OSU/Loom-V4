#pragma once

#include <SparkFun_AS7265X.h>
#include <Wire.h>

#include "../I2CDevice.h"
#include "Loom_Manager.h"

/**
 * AS7265X Full Spectrum Sensor
 * 
 * @author Will Richards
 */ 
class Loom_AS7265X : public I2CDevice{
    protected:
        
        void power_down() override {}; 
        void print_measurements() override {};
       
        // Manager controlled functions
        void measure() override;                               
        void initialize() override;    
        void package() override;   
        void power_up() override;

    public:
        /**
         * Constructs a new AS7265X sensor
         * @param man Reference to the manager that is used to universally package all data
         * @param useMux If this module will be using the mux
         * @param addr I2C address that is assigned to the sensor
         * @param use_bulb Enable the light bulb
         * @param gain Gain level
         * @param mode Read Mode: 0("4 channels out of 6"), 1("Different 4 channels out of 6"), 2("All 6 channels continuously"), 3("One-shot reading of all channels") 
         * @param integration_time Integration time (time will be 2.8ms * [integration value])
         */ 
        Loom_AS7265X(
                    Manager& man,
                    bool useMux = false, 
                    int addr = 0x49,
                    bool use_bulb = false,
                    uint8_t gain = 64,
                    uint8_t mode = AS7265X_MEASUREMENT_MODE_6CHAN_ONE_SHOT,
                    uint8_t integration_time = 50
                );

        /* Returns a pointer to the UV array, it is 6 elements*/
        uint16_t* getUV() { return uv; };
        
        /* Returns a pointer to the color array, it is 6 elements*/
        uint16_t* getColor() { return color; };

        /* Returns a pointer to the NIR array, it is 6 elements*/
        uint16_t* getNIR() { return nir; };
    private:
        Manager* manInst;                       // Instance of the manager
        AS7265X asInst;                         // Instance of the AS7265X
        
        uint16_t	uv[6];				        // Measured UV bands values. Units: counts / (μW/cm^2)
        uint16_t	color[6];			        // Measured color bands values. Units: counts / (μW/cm^2)
        uint16_t	nir[6];				        // Measured near-infra-red bands values. Units: counts / (μW/cm^2)

        bool		use_bulb;			        // Whether or not to use the bulb

        uint8_t		gain;				        // Gain setting
        uint8_t		mode;				        // Sensor mode
        uint8_t		integration_time;	        // Integration time setting
};