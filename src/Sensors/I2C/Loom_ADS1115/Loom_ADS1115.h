#pragma once

#include <Adafruit_ADS1X15.h>
#include <map>

#include "../I2CSensor.h"
#include "Loom_Manager.h"

/**
 * Functionality for the ADS1115
 * 
 * @author Will Richards
 */  
class Loom_ADS1115 : public I2CSensor{


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
         *  @param useMux Whether or not to use the mux
         *  @param address I2C address to communicate over
         *  @param enable_analog If we want to read the analog data from the ADS1115
         *  @param enable_diff If we want to read the differential data from the sensor
         *  @param gain How much gain to apply to the readings.
         */ 
        Loom_ADS1115(
                Manager& man,
                byte address            = ADS1X15_ADDRESS,
                bool useMux             = false, 
                bool enable_analog      = true,
                bool enable_diff        = false,
                adsGain_t gain          = adsGain_t::GAIN_ONE
            );


        /**
         * Get the value in the analog table according to analog number not INDEX
         * @param pin Pin to get data from (1-4)
         */ 
        int getAnalog(int pin) { return analogData[pin-1]; };

        /**
         * Get the value in the diff table according to diff number not INDEX
         * @param pin Pin to get data from (1-2)
         */ 
        int getDiff(int pin) { return analogData[pin-1]; };
    
    private:
        Manager* manInst;                                           // Instance of the manager
        Adafruit_ADS1115 ads;                                       // Instance of the ADS1115 Library

        adsGain_t adc_gain;                                         // Gain to set the amplifier to
        byte i2c_address;                                           // I2C address
        bool enableAnalog;                                          // Read from the analog pins
        bool enableDiff;                                            // Read differentials

        int16_t analogData[4];                                      // Stores the analog ADS1115 data
        int16_t diffData[2];                                        // Stores the differential data from the sensor
        float volts[4];						                        // Stores Computed Voltage Conversions
};