#pragma once

#include <Adafruit_TSL2591.h>
#include <Adafruit_Sensor.h>

#include "Module.h"
#include "Loom_Manager.h"

/**
 * TSL2591 Light Sensor
 * 
 * @author Will Richards
 */ 
class Loom_TSL2591 : public Module{
    protected:
       
       // Manager controlled functions
        void measure() override;                               
        void print_measurements() override;
        void initialize() override;    
        void power_up() override;
        void power_down() override {}; 
        void package() override;   

    public:
        /**
         * Constructs a new TSL2591 sensor
         * @param man Reference to the manager that is used to universally package all data
         * @param address I2C address that is assigned to the sensor
         * @param light_gain Sets the gain level for how much to amplify the input
         * @param integration_time How long we want to integrate the data, longer time results in dimmer values
         */ 
        Loom_TSL2591(
                      Manager& man,
                      int address = 0x29, 
                      bool useMux = false, 
                      tsl2591Gain_t light_gain = TSL2591_GAIN_MED, 
                      tsl2591IntegrationTime_t integration_time = TSL2591_INTEGRATIONTIME_100MS
                );

        /**
         * Get recorded visible light
         */ 
        uint16_t getVisible() {return lightLevels[0]; };

        /**
         * Get recorded infrared light
         */ 
        uint16_t getInfrared() { return lightLevels[1]; };

        /**
         * Get recorded full spectrum
         */ 
        uint16_t getFullSpectrum() {return lightLevels[2]; };



    private:
        Manager* manInst;                       // Instance of the manager
        Adafruit_TSL2591 tsl;                   // Adafruit TSL2591 Sensor Object

        uint16_t lightLevels[3] = {0, 0, 0};    // Array of size 3 to hold all collected data

        tsl2591Gain_t gain;                     // Light level gain
        tsl2591IntegrationTime_t intTime;       // Integration time of the sensor (longer time = dimmer)
};