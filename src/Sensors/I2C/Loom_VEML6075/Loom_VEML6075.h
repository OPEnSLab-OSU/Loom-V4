#pragma once

#include "../I2CDevice.h"
#include "Loom_Manager.h"
#include <Wire.h>

#include <Adafruit_VEML6075.h>
#include <Adafruit_Sensor.h>

/**
 * Interface for the VEML6075 ultraviolet light sensor module.
 * 
 * @author James Tappert
 */ 

class Loom_VEML6075 : public I2CDevice{
    protected:
       
       // Manager controlled functions
        void measure() override;                               
        void initialize() override;    
        void power_up() override {};
        void power_down() override {}; 
        void package() override;   

    public:
        /**
         * Constructs a new VEML6075 sensor
         * @param man Reference to the manager that is used to universally package all data
         * @param address I2C address that is assigned to the sensor
         */ 
        Loom_VEML6075(
                      Manager& man,
                      int address = 0x10, 
                      bool useMux = false 
                );

        /**
         * Get Ultraviolet-A value
         */ 
        float readUVA() { return UVA; }

        /**
         * Get Ultraviolet-B value
         */ 
        float readUVB() { return UVB; }

        /**
         * Get Ultraviolet Index value
         */ 
        float readUVI() { return UVI; }


    private:
        Manager* manInst;                       // Instance of the manager
        Adafruit_VEML6075 veml;                 // Adafruit VENL6075 Sensor Object                  
        float UVA;                              // Ultraviolet-A value (315nm - 400nm)
        float UVB;                              // Ultraviolet-B value (280nm - 315nm)
        float UVI;                              // Ultraviolet Index value
 
        bool initialized = true;                // True until set to false
};