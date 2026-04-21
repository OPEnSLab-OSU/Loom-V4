#pragma once

#include "../I2CDevice.h"
#include "Loom_Manager.h"
#include <Wire.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_VCNL4020.h>

/**
 * Loom integration for VCNL4020 Proximity and Ambient Light sensor module. 
 * 
 * Proximity =  16-bit counts (ADC output)
 * Ambient Light = 16-bit ALS measurement (ADC output)
 * 
 * @author Shawn Singharaj 
*/

class Loom_VCNL4020 : public I2CDevice {
    protected:
        // Manager lifecycle functions
        void initialize() override;
        void measure() override;
        void package() override;
        void power_up() override;
        void power_down() override {};

    public:
        /**
         * VCNL4020 Constructor
         * @param managerInstance reference of the manager to run manager lifecycle functions
         * @param address 0x13 by default
         * @param ambRate, ambAvg, proxLED, proxRate, proxFreq are config setting for sensor upon manager.power_up()
         */
        Loom_VCNL4020(Manager &managerInstance, int address = 0x13, bool useMux = false,
                        vcnl4020_ambientrate ambRate = AMBIENT_RATE_10_SPS, 
                        vcnl4020_averaging ambAvg = AVG_1_SAMPLES,
                        uint8_t proxLED = 200, 
                        vcnl4020_proxrate proxRate = PROX_RATE_250_PER_S, 
                        vcnl4020_proxfreq proxFreq = PROX_FREQ_390_625_KHZ);

        // Read the ambient light level
        uint16_t readAmbient() { return ambientLight; }

        // Read the proximity level
        uint16_t readProximity() { return proximity; }

    private:
        Manager *managerInstance;           // Manager instance
        Adafruit_VCNL4020 vcnl;             // Adafruit's VCNL4020 object
        uint16_t ambientLight;              // Ambient illumination value
        uint16_t proximity;                 // Proximity value

        // Config settings for power_up
        vcnl4020_ambientrate ambRate;
        vcnl4020_averaging ambAvg;
        vcnl4020_proxrate proxRate;
        uint8_t proxLED;
        vcnl4020_proxfreq proxFreq;

        bool initialized = true; // True until set to false
};