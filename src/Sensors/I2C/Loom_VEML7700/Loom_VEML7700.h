#pragma once

#include "../I2CDevice.h"
#include "Loom_Manager.h"
#include <Wire.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_VEML7700.h>

/**
 * Loom integration for VEML7700 light illuminance (lux) sensor module. 
 * Shawn Singharaj 
*/


class Loom_VEML7700 : public I2CDevice {
    protected:
    // Manager lifecycle functions
    void initialize() override;
    void measure() override;
    void package() override;
    void power_up() override;
    void power_down() override {};

    public:
    /**
     * Sensor module constructor for new VEML7700
     * parameters: man - Manager reference to standardize sensor functionalities
     *             address - I2C address assigned to sensor
     */
    Loom_VEML7700(Manager &man, int address = 0x10, bool useMux = false,
    uint8_t gain = VEML7700_GAIN_1_8, uint8_t int_time = VEML7700_IT_100MS,
    uint8_t persistence = VEML7700_PERS_1);

    // Get Lux illumination value
    float readLux(luxMethod method = VEML_LUX_NORMAL) { return veml.readLux(method); }

    private:
        Manager *managerInstance;           // Manager instance
        Adafruit_VEML7700 veml; // Adafruit's VEML7700 object
        float Lux;              // Lux illumination value

        uint8_t gain;
        uint8_t int_time;
        uint8_t persistence;

        bool initialized = true; // True until set to false
};