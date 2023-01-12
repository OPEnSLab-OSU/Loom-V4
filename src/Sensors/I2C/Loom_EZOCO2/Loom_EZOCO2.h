#pragma once

#include "../EZO/EZOSensor.h"
#include "Loom_Manager.h"

/**
 * Functionality for the EZO CO2 Sensor
 * 
 * @author Will Richards
 */  
class Loom_EZOCO2 : public EZOSensor{
    protected:
        void print_measurements() override {};  
        void power_up() override {}; 

    public:
        void initialize() override;
        void measure() override;
        void package() override;
        void power_down() override;

        /**
         *  Construct a new EZCO2 device
         *  @param man Reference to the manager
         *  @param address I2C address to communicate over
         *  @param useMux Whether or not to use the mux
         */ 
        Loom_EZOCO2(
                Manager& man,
                byte address            = 0x69,
                bool useMux             = false
            );


        /**
         * Get the dissolved oxygen level
         */ 
        float getCO2() { return co2; };
    
    private:
        Manager* manInst;                                                    // Instance of the manager
        
        float co2;                                                           // Reading for the DO value

        
        

       
};