#pragma once

#include "../EZO/EZOSensor.h"
#include "Loom_Manager.h"

/**
 * Functionality for the EZO RGB Sensor
 * 
 * @author Will Richards
 */  
class Loom_EZORGB : public EZOSensor{
    protected:
        
        void power_up() override {}; 

    public:
        void initialize() override;
        void measure() override;
        void package() override;
        void power_down() override;

        /**
         *  Construct a new EZORGB device
         *  @param man Reference to the manager
         *  @param address I2C address to communicate over
         *  @param useMux Whether or not to use the mux
         */ 
        Loom_EZORGB(
                Manager& man,
                byte address            = 0x70,
                bool useMux             = false
            );


        /**
         * Get the color values individually
         */ 
        uint8_t getRed() { return rgb[0]; };
        uint8_t getGreen() { return rgb[1]; };
        uint8_t getBlue() { return rgb[2]; };
    
    private:
        Manager* manInst;                                                       // Instance of the manager
        
        uint8_t rgb[3] = {0,0,0};                                               // RGB readings
        void parseData(String sensorData);                                      // Parse data into a separated format

       
};