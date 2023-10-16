#pragma once

#include <SensirionI2CSen5x.h>
#include <Wire.h>

#include "../I2CDevice.h"
#include "Loom_Manager.h"

/**
 *  SEN55 Air Quality sensors, supports pm 1.0, 2.5, 4.0, 10 as well as Temp/Humidity and Nox and Voc index
 * 
 * NOTE: To get accurate results using the SE555 it should be powered on and remain on as according to the data sheet,
 * the switch-on behavior for the VOC Index is ~ 1 hr and the NOx is ~ 6 hours.
 * Data sheet: https://cdn.sparkfun.com/assets/5/b/f/2/8/Sensirion_Datasheet_SEN5x.pdf (Page 8)
 * 
 *  @author Will Richards
 */ 
class Loom_SEN55 : public I2CDevice{
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
        Loom_SEN55(
                    Manager& man,
                    bool measurePM = true,
                    bool useMux = false
                );

        /**
         * Adjust the temperature reading offset in celsius. By default it accounts for the internal heating of the sensor.
         * Adjustments should be made based off this datasheet: https://cdn.sos.sk/productdata/a8/47/608a6927/sen54-sdn-t.pdf
         * 
         * @param offset The additional temperature offset in degrees celsius
        */
        void adjustTempOffset(float offset);

        /**
         * Copied directly from the SEN55 library
         * setVocAlgorithmTuningParameters() - Sets the tuning parameters of the VOC
         * algorithm.
         *
         * Supported sensors: SEN54, SEN55
         *
         * @note This command is available only in idle mode. In measure mode, this
         * command has no effect. In addition, it has no effect if at least one
         * parameter is outside the specified range.
         *
         * @param indexOffset VOC index representing typical (average) conditions.
         * Allowed values are in range 1..250. The default value is 100.
         *
         * @param learningTimeOffsetHours Time constant to estimate the VOC
         * algorithm offset from the history in hours. Past events will be forgotten
         * after about twice the learning time. Allowed values are in range 1..1000.
         * The default value is 12 hours.
         *
         * @param learningTimeGainHours Time constant to estimate the VOC algorithm
         * gain from the history in hours. Past events will be forgotten after about
         * twice the learning time. Allowed values are in range 1..1000. The default
         * value is 12 hours.
         *
         * @param gatingMaxDurationMinutes Maximum duration of gating in minutes
         * (freeze of estimator during high VOC index signal). Set to zero to
         * disable the gating. Allowed values are in range 0..3000. The default
         * value is 180 minutes.
         *
         * @param stdInitial Initial estimate for standard deviation. Lower value
         * boosts events during initial learning period, but may result in larger
         * device-to-device variations. Allowed values are in range 10..5000. The
         * default value is 50.
         *
         * @param gainFactor Gain factor to amplify or to attenuate the VOC index
         * output. Allowed values are in range 1..1000. The default value is 230.
         *
         * @return 0 on success, an error code otherwise
         */
        uint16_t setVocAlgorithmTuningParameters(int16_t indexOffset,
                                                int16_t learningTimeOffsetHours,
                                                int16_t learningTimeGainHours,
                                                int16_t gatingMaxDurationMinutes,
                                                int16_t stdInitial,
                                                int16_t gainFactor) { return sen5x.setVocAlgorithmTuningParameters(indexOffset, learningTimeOffsetHours, learningTimeGainHours, gatingMaxDurationMinutes, stdInitial, gainFactor);};


         /**
         * setNoxAlgorithmTuningParameters() - Sets the tuning parameters of the NOx
         * algorithm.
         *
         * Supported sensors: SEN55
         *
         * @note This command is available only in idle mode. In measure mode, this
         * command has no effect. In addition, it has no effect if at least one
         * parameter is outside the specified range.
         *
         * @param indexOffset NOx index representing typical (average) conditions.
         * Allowed values are in range 1..250. The default value is 1.
         *
         * @param learningTimeOffsetHours Time constant to estimate the NOx
         * algorithm offset from the history in hours. Past events will be forgotten
         * after about twice the learning time. Allowed values are in range 1..1000.
         * The default value is 12 hours.
         *
         * @param learningTimeGainHours The time constant to estimate the NOx
         * algorithm gain from the history has no impact for NOx. This parameter is
         * still in place for consistency reasons with the VOC tuning parameters
         * command. This parameter must always be set to 12 hours.
         *
         * @param gatingMaxDurationMinutes Maximum duration of gating in minutes
         * (freeze of estimator during high NOx index signal). Set to zero to
         * disable the gating. Allowed values are in range 0..3000. The default
         * value is 720 minutes.
         *
         * @param stdInitial The initial estimate for standard deviation parameter
         * has no impact for NOx. This parameter is still in place for consistency
         * reasons with the VOC tuning parameters command. This parameter must
         * always be set to 50.
         *
         * @param gainFactor Gain factor to amplify or to attenuate the NOx index
         * output. Allowed values are in range 1..1000. The default value is 230.
         *
         * @return 0 on success, an error code otherwise
         */
        uint16_t setNoxAlgorithmTuningParameters(int16_t indexOffset,
                                                int16_t learningTimeOffsetHours,
                                                int16_t learningTimeGainHours,
                                                int16_t gatingMaxDurationMinutes,
                                                int16_t stdInitial,
                                                int16_t gainFactor) { return sen5x.setNoxAlgorithmTuningParameters(indexOffset, learningTimeOffsetHours, learningTimeGainHours, gatingMaxDurationMinutes, stdInitial, gainFactor); };
        /**
         * Get the PM 1.0 reading
         */ 
        float getPM1() { return massConcentrationPm1p0; };

        /**
         * Get the PM2.5 reading
        */
        float getPM25() { return massConcentrationPm2p5; };

        /**
         * Get the PM4.0 reading
        */
        float getPM4() { return massConcentrationPm4p0; };
        
        /**
         * Get the PM 10 reading
        */
        float getPM10() { return massConcentrationPm10p0; };
        
        /**
         * Get the PM2.5 reading
        */
        float getHumidity() { return ambientHumidity; };

        /**
         * Get the temperature reading
         */ 
        float getTemperature() { return ambientTemperature; };

        /**
         * Get the VOX Index reading
         */ 
        float getVOCIndex() { return vocIndex; };

        /**
         * Get NOX Index
        */
        float getNOXIndex() { return noxIndex; };

    private:
        Manager* manInst;                           // Instance of the manager
        SensirionI2CSen5x sen5x;                    // Instance of the SEN55 object

        bool measurePM;                             // Are we measuring particulate matter or not

        /* Sensor readings */
        float massConcentrationPm1p0;
        float massConcentrationPm2p5;
        float massConcentrationPm4p0;
        float massConcentrationPm10p0;
        float ambientHumidity;
        float ambientTemperature;
        float vocIndex;
        float noxIndex;

       
};