#pragma once

#include <Adafruit_MAX31856.h>

#include "Module.h"
#include "Loom_Manager.h"


/**
 * Class for managing the MAX31856 NOT TO BE CONFUSED WITH THE MAX31865
 * 
 * @author Will Richards
 */ 
class Loom_MAX31856 : public Module{
    protected:
        
        void power_up() override {};
        void power_down() override {}; 
        
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
        Loom_MAX31856(Manager& man, int samples = 1, int chip_select = 10, int mosi = -1, int miso = -1, int sclk = -1, bool farenheit = false);

        ~Loom_MAX31856()
        {
            delete maxthermo;
        };

        /**
         * Get the recorded temperature
         */ 
        float getTemperature() { return temperature; };

    private:
        Manager* manInst;           // Instance of the manager

        Adafruit_MAX31856* maxthermo = nullptr;      // Instance of the MAX31856 library
        int num_samples;            // Number of samples to take and average
        bool farenheit_display;

        float temperature = 0;      // Temperature that will be packaged
};