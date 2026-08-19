#pragma once

#include "SensirionI2cSen66.h"
#include <Wire.h>
#include <bitset>

#include "../I2CDevice.h"
#include "Loom_Manager.h"

#define PM_AVERAGE_COUNT 10     // Number of times to read the pm values then average them over
#define SEN66_I2C_ADDRESS 0x6B  // Standard I2C address for SEN66

/**
 * SEN66 Air Quality Sensor
 * Supports: PM 1.0, 2.5, 4.0, 10.0, Humidity, Temperature, VOC Index, NOx Index, and CO2.
 *
 * NOTE: The SEN66 reads PM, Gas, and CO2 in a single block. 
 *
 * @author Soren Emmons / Adapted for SEN66
 */
class Loom_SEN66 : public I2CDevice{
    protected:

       // Manager controlled functions
        void measure() override;
        void initialize() override;
        void power_up() override {};
        void power_down() override {};
        void package() override;

    public:
        /**
         * Constructs a new SEN66 sensor
         * * Note: SEN66 addr: 0x6B
         *
         * @param man Reference to the manager that is used to universally package all data
         * @param measurePM Sets whether we should package PM data (Note: SEN66 reads PM internally regardless)
         * @param useMux Whether or not we are using the multiplexer class
         * @param readNumVals Whether we should read and log number concentration values
         */
        Loom_SEN66(
                    Manager& man,
                    bool measurePM = true,
                    bool useMux = false,
                    bool readNumVals = true
                );

        /**
         * Adjust the temperature reading offset.
         *
         * @param offset Temperature offset
         * @param slope Temperature slope (default 0)
         * @param timeConstant Time constant (default 0)
         */
        void adjustTempOffset(int16_t offset, int16_t slope = 0, uint16_t timeConstant = 0);

        /**
         * Sets tuning parameters for the VOC Algorithm.
         */
        uint16_t setVocAlgorithmTuningParameters(int16_t indexOffset,
                                                int16_t learningTimeOffsetHours,
                                                int16_t learningTimeGainHours,
                                                int16_t gatingMaxDurationMinutes,
                                                int16_t stdInitial,
                                                int16_t gainFactor) { 
            return sen66.setVocAlgorithmTuningParameters(indexOffset, learningTimeOffsetHours, learningTimeGainHours, gatingMaxDurationMinutes, stdInitial, gainFactor);
        };

         /**
         * Sets tuning parameters for the NOx Algorithm.
         */
        uint16_t setNoxAlgorithmTuningParameters(int16_t indexOffset,
                                                int16_t learningTimeOffsetHours,
                                                int16_t learningTimeGainHours,
                                                int16_t gatingMaxDurationMinutes,
                                                int16_t stdInitial,
                                                int16_t gainFactor) { 
            return sen66.setNoxAlgorithmTuningParameters(indexOffset, learningTimeOffsetHours, learningTimeGainHours, gatingMaxDurationMinutes, stdInitial, gainFactor); 
        };
        
        // Getters
        float getPM1() { return massConcentrationPm1p0; };
        float getPM25() { return massConcentrationPm2p5; };
        float getPM4() { return massConcentrationPm4p0; };
        float getPM10() { return massConcentrationPm10p0; };
        float getHumidity() { return ambientHumidity; };
        float getTemperature() { return ambientTemperature; };
        float getVOCIndex() { return vocIndex; };
        float getNOXIndex() { return noxIndex; };
        uint16_t getCO2() { return co2; };

        /**
         * Logs device register status 
         */
       void logDeviceStatus();

        /**
        * Reset relevant values to 0, useful for preparing for another measurement cycle.
        */
        void resetValuesForMeasure();

    private:
        Manager* manInst;                       // Instance of the manager
        SensirionI2cSen66 sen66;                // Instance of the SEN66 driver object

        bool measurePM;                         // Are we logging particulate matter
        bool readNumVals;                       // Do we want to read the number concentration?

        /* Sensor readings */
        float massConcentrationPm1p0;
        float massConcentrationPm2p5;
        float massConcentrationPm4p0;
        float massConcentrationPm10p0;
        float ambientHumidity;
        float ambientTemperature;
        float vocIndex;
        float noxIndex;
        uint16_t co2;

        /* PM number readings */
        float numConcentrationPm0p5;
        float numConcentrationPm1p0;
        float numConcentrationPm2p5;
        float numConcentrationPm4p0;
        float numConcentrationPm10p0;
};