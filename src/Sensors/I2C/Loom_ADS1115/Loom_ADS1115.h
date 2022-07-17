#pragma once

#include <Adafruit_ADS1X15.h>
#include <map>

#include "Module.h"
#include "Loom_Manager.h"

#include "../../../Utilities/Primative.h"



/**
 * Functionality for the ADS1115
 */  
class Loom_ADS1115 : public Module{

    // Function signature to handle custom calculations for individual projects
    typedef void (*calcFunction)(Primative& prim, int16_t analog[], int16_t diff[]);

    protected:
        void print_measurements() override {};  
        void power_up() override {};
        void power_down() override {}; 

    public:
        void initialize() override;
        void measure() override;
        void package() override;

        /**
         *  Construct a new ADS1115
         *  @param man Reference to the manager
         *  @param address I2C address to communicate over
         *  @param enable_analog If we want to read the analog data from the ADS1115
         *  @param enable_diff If we want to read the differential data from the sensor
         *  @param gain How much gain to apply to the readings.
         */ 
        Loom_ADS1115(
                Manager& man, 
                byte address            = ADS1X15_ADDRESS,
                bool enable_analog      = true,
                bool enable_diff        = false,
                adsGain_t gain          = adsGain_t::GAIN_TWOTHIRDS
            );

        /**
         * Pass a function in as a lambda to return the calculation we want, should be done in set
         * The format for the function to be used for The ADS1115 is as follows 
         * void funcName(Primative& prim, int16_t analog[4] , int16_t diff[2])
         * @param func Function used to produce the calculated result
         * @param keyName Name to store the custom calculation under
         * @return This is determined at compile time
         */ 
        void addCustomCalculation(calcFunction func, String keyName);
    
    private:
        Manager* manInst;                                           // Instance of the manager
        Adafruit_ADS1115 ads;                                       // Instance of the ADS1115 Library

        adsGain_t adc_gain;                                         // Gain to set the amplifier to
        byte i2c_address;                                           // I2C address
        bool enableAnalog;                                          // Read from the analog pins
        bool enableDiff;                                            // Read differentials

        int16_t analogData[4];                                      // Stores the analog ADS1115 data
        int16_t diffData[2];                                        // Stores the differential data from the sensor

        std::map<String, calcFunction> customCalculations;    // Maps a key name to a custom calculation function to run

        bool initialized = true;

};