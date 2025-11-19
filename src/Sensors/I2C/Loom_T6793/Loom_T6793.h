#pragma once

#include <Wire.h>
#include <bitset>

#include "../I2CDevice.h"
#include "Loom_Manager.h"

#define CO2_AVERAGE_COUNT 10

/**
 *  The T6793-5K supports CO@ measurements up to 5k ppm
 *
 * Data sheet: https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/6155/AAS920685H.pdf
 *
 *  @author Douglas Crocker
 */
class Loom_T6793 : public I2CDevice{
    protected:

       // Manager controlled functions
        void measure() override;
        void initialize() override;
        void power_up() override {};
        void power_down() override {};
        void package() override;

    public:
        /**
         * Constructs a new SEN55 sensor
         *
         * @param man Reference to the manager that is used to universally package all data
         * @param addr Address of sensor
         * @param readDelay Delay for reading from I2C bus
         * @param useMux True if should use mux to aquire address
         */
        Loom_T6793(
                    Manager& man,
                    uint8_t addr = 0x15,
                    uint8_t readDelay = 10, // Delay for reading Wire in ms
                    bool useMux = false
                );

        /**
         * Get Status of Current T6793 Sensor
         *
        */
        bool GetSensorStatus();

        /**
         * Get Serial Number of Current T6793 Sensor
         *
        */
        unsigned long GetSerialNo();

    private:
        Manager* manInst;                           // Instance of the manager
        uint8_t i2s_addr;

        uint8_t wireReadDelay;
        float CO2_Val;


};