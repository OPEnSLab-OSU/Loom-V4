#pragma once

#include <Adafruit_seesaw.h>

#include "Module.h"
#include "Loom_Manager.h"

/**
 * STEMMA Soil Moisture sensor
 * 
 * @author Will Richards
 */ 
class Loom_STEMMA : public Module{
    protected:
       
        // Manager controlled functions
        void measure() override;                               
        void print_measurements() override;
        void initialize() override;    
        void power_up() override {};
        void power_down() override {}; 
        void package() override;   

    public:
        /**
         * Constructs a new TSL2591 sensor
         * @param man Reference to the manager that is used to universally package all data
         * @param address I2C address that is assigned to the sensor
         */ 
        Loom_STEMMA(
                    Manager& man,
                    bool useMux = false, 
                    int addr = 0x36
                );

        /**
         * Get temperature
         */ 
        float getTemperature() {return temperature; };

        /**
         * Get recorded infrared light
         */ 
        uint16_t getCapacitive() { return cap; };



    private:
        Manager* manInst;                       // Instance of the manager
        Adafruit_seesaw stemma;                 // Adafruit STEMMA Sensor Object
        int address;

        float temperature;                      // Soil temperature
        uint16_t cap;                           // Soil capacitive 
};