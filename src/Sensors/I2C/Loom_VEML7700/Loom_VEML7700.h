#pragma once

#include <Adafruit_VEML7700.h>
#include <Adafruit_Sensor.h>

#include "../I2CDevice.h"
#include "Loom_Manager.h"

class Loom_VEML7700 : public I2CDevice{
    protected:

       // Manager controlled functions
        void measure() override;
        void initialize() override;
        void power_up() override {};
        void power_down() override {};
        void package() override;

    public:
        /**
         * Constructs a new VEML7700 sensor
         * @param man Reference to the manager that is used to universally package all data
         * @param address I2C address that is assigned to the sensor
         */
    Loom_VEML7700(
                    Manager& man,
                    int address = 0x10,
                    bool useMux = false
                );
    /**
    * Get recorded lux value
    */
    float getLux() {return autoLux; };

    /**
    * Get recorded ALS value
    */
    uint16_t getALS() {return rawALS; };

    /**
    * Get recorded white light value
    */
    uint16_t getWhite() {return rawWhite; };

    private:
     Manager* manInst;              // Instance of the manager
     Adafruit_VEML7700 veml;        //VEML7700 sensor object

     float autoLux;                 // Lux value from sensor
     uint16_t rawALS;               // Raw ALS value
     uint16_t rawWhite;             // Raw white light value
};