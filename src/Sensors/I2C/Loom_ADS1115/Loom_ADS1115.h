#pragma once

#include <Adafruit_ADS1X15.h>
#include <map>

#include "../I2CDevice.h"
#include "Loom_Manager.h"

enum class AnalogChannelEnable {
    ENABLE_NONE,
    ENABLE_1,
    ENABLE_1_2,
    ENABLE_1_2_3,
    ENABLE_1_2_3_4
};

/**
 * Functionality for the ADS1115
 * 
 * @author Will Richards
 */  
class Loom_ADS1115 : public I2CDevice{
    protected:
        
        void power_down() override {}; 

    public:
        void initialize() override;
        void measure() override;
        void package() override;
        void power_up() override;

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
                AnalogChannelEnable     enable_analog  = AnalogChannelEnable::ENABLE_1_2_3_4,
                bool enableVolts        = false,
                bool enable_diff        = false,
                adsGain_t gain          = adsGain_t::GAIN_ONE
            );


        /**
         * Get the value in the analog table according to analog number not INDEX
         * @param pin Pin to get data from (1-4)
         */ 
        float getAnalog(int pin) { return (float)analogData[pin-1]; };

        /**
         * Get the value in the diff table according to diff number not INDEX
         * @param pin Pin to get data from (1-2)
         */ 
        float getDiff(int pin) { return (float)analogData[pin-1]; };
    
    private:
        Manager* manInst;                                           // Instance of the manager
        Adafruit_ADS1115 ads;                                       // Instance of the ADS1115 Library

        adsGain_t adc_gain;                                         // Gain to set the amplifier to
        byte i2c_address;                                           // I2C address
        AnalogChannelEnable enableAnalog;                                          // Read from the analog pins
        bool enableDiff;                                            // Read differentials
        bool enableVolts;                                            // Read differentials

        uint16_t analogData[4];                                      // Stores the analog ADS1115 data
        uint16_t diffData[2];                                        // Stores the differential data from the sensor
        float volts[4];						                        // Stores Computed Voltage Conversions
};