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
 * Shawn Singharaj 
*/

class Loom_VCNL4020 :: public I2CDevice {
    protected:
        void initialize() override;
        void measure() override;
        void package() override;
        void power_up() override;
        void power_down() override {};

    public:
        Loom_VCNL4020(Manager &managerInstance, int address = 0x13, bool useMux = false,
                        ambRate = AMBIENT_RATE_10_SPS, ambAvg = AVG_1_SAMPLES,
                        proxLED = 200, proxRate = PROX_RATE_250_PER_S, proxFreq = PROX_FREQ_390_625_KHZ);

        uint16_t readAmbient() { return ambientLight; }

        uint16_t readProximity() { return proximity; }

    private:
        Manager *managerInstance;           // Manager instance
        Adafruit_VCNL4020 vcnl;             // Adafruit's VCNL4020 object
        uint16_t ambientLight;              // Lux illumination value
        uint16_t proximity;

        uint8_t ambRate;
        uint8_t ambAvg;
        uint8_t proxRate;
        uint8_t proxLED;
        uint8_t proxFreq;

        bool initialized = true; // True until set to false

}