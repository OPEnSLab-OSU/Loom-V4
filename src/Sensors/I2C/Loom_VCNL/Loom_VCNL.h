#pragma once

#include "../I2CDevice.h"
#include "Loom_Manager.h"
#include <Wire.h>

#include <Adafruit_VCNL4010.h>
#include <Adafruit_Sensor.h>

/**
 * Interface for the VCNL4010 Proximity & Ambient light sensor module.
 * 
 * @author Will Richards
 */ 

class Loom_VCNL : public I2CDevice{
    protected:
       
       // Manager controlled functions
        void measure() override;                               
        void initialize() override;    
        void power_up() override {};
        void power_down() override {}; 
        void package() override;   

    public:
        /**
         * Constructs a new vcnl4010 sensor
         * @param man Reference to the manager that is used to universally package all data
         * @param address I2C address that is assigned to the sensor
         */ 
        Loom_VCNL(
                      Manager& man,
                      int address = 0x13, 
                      bool useMux = false 
                );

        /**
         * Get ambient light
         */ 
        uint16_t readAmbient() { return ambientLight; }

        /**
         * Get proximity
         */ 
        uint16_t readProximity() { return proximity; }


    private:
        Manager* manInst;                       // Instance of the manager
        VCNL4010 vcnl;                          // Adafruit VCNL4010 Sensor Object

         uint16_t ambientLight = 0;             // ambient light value
         uint16_t proximity = 0;                // proximity value

        bool initialized = true;                // True until set to false
};