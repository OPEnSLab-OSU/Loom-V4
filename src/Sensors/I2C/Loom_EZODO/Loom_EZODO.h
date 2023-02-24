#pragma once

#include "../EZO/EZOSensor.h"
#include "Loom_Manager.h"

/**
 * Functionality for the EZO Dissolved Oxygen Sensor
 * 
 * @author Will Richards
 */  
class Loom_EZODO : public EZOSensor{
    protected:
        
        void power_up() override {}; 

    public:
        void initialize() override;
        void measure() override;
        void package() override;
        void power_down() override;

        /**
         *  Construct a new EZODO device
         *  @param man Reference to the manager
         *  @param address I2C address to communicate over
         *  @param useMux Whether or not to use the mux
         */ 
        Loom_EZODO(
                Manager& man,
                byte address            = 0x61,
                bool useMux             = false
            );


        /**
         * Get the dissolved oxygen level
         */ 
        float getOxygen() { return oxygen; };

        /**
         * Get the percent saturation
         */ 
        float getSaturation() { return saturation; };
    
    private:
        Manager* manInst;                                                       // Instance of the manager
        
        float oxygen;                                                           // Reading for the DO value
        float saturation;                                                       // Get the percent saturation
        void parseResponse(String response);
       
};