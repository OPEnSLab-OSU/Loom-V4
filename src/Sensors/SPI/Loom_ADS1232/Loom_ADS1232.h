#pragma once

#include "Module.h"
#include "Loom_Manager.h"

#include <ADS1232_Lib.h>


/**
 * ADS1232 Weight Sensor
 * 
 * @author Will Richards
 */ 
class Loom_ADS1232 : public Module{
    protected:
         

    public:
        Loom_ADS1232(Manager& man, int num_samples = 1, long offset = 8403613, float scale = 2041.46);

        void initialize() override;
        void power_up() override;
        void power_down() override;
        void measure() override;
        void package() override;

        /**
         * Calibrate the sensor
         */ 
        void calibrate();

        /**
         * Get the recorded weight
         */ 
        float getWeight() { return weight; };
        float getRaw() {return inst.raw_read(8); };

    private:
        Manager* manInst;       // Instance of the Manager

        ADS1232_Lib inst;       // Instance of the library

        float weight;           // Weight output

        long offset;            // Calibration offset
        float scale;            // Calibration scale
        int num_samples;        // Number of samples to collect

};