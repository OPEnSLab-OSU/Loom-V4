#pragma once

#include <Wire.h>
#include <bitset>

#include "../I2CDevice.h"
#include "Loom_Manager.h"

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
         * @param measurePM This sets whether or not we should conduct measurements without PM readings (uses less power)
         * @param useMux Whether or not we are using the multiplexer class
         */
        Loom_T6793(
                    Manager& man,
                    uint8_t addr = 0x15,
                    uint8_t readDelay = 10, // Delay for reading Wire in ms
                    bool useMux = false
                );

    private:
        Manager* manInst;                           // Instance of the manager
        uint8_t i2s_addr;

        uint8_t wireReadDelay;
        float CO2_Val;


};