#pragma once

#include <Adafruit_MAX31856.h>

#include "Module.h"
#include "Loom_Manager.h"


/**
 * Class for managing the MAX31865 NOT TO BE CONFUSED WITH THE MAX31865
 */ 
class Loom_MAX31856 : public Module{
    public:
        void initialize() override;
        void measure() override;
        void package() override;

        /**
         * Construct a new sensor class
         * @param man Reference to the manager
         * @param chip_select What pin SPI pin to use
         * @param num_samples The number of samples to collect and average
         */ 
        Loom_MAX31856(Manager& man, int chip_select = 10, int samples = 1);

    private:
        Manager* manInst;           // Instance of the manager

        Adafruit_MAX31856 max;      // Instance of the MAX31865 library
        int num_samples;            // Number of samples to take and average

        float temperature = 0;      // Temperature that will be packaged
};