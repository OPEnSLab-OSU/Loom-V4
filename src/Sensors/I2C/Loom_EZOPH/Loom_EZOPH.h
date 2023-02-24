#pragma once

#include "../EZO/EZOSensor.h"
#include "Loom_Manager.h"

/**
 * Functionality for the EZO PH Sensor
 * 
 * @author Will Richards
 */  
class Loom_EZOPH : public EZOSensor{
    protected:
        
        void power_up() override {}; 

    public:
        void initialize() override;
        void measure() override;
        void package() override;
        void power_down() override;

        /**
         *  Construct a new EZOPH device
         *  @param man Reference to the manager
         *  @param address I2C address to communicate over
         *  @param useMux Whether or not to use the mux
         */ 
        Loom_EZOPH(
                Manager& man,
                byte address            = 0x63,
                bool useMux             = false
            );


        /**
         * Get the PH level
         */ 
        float getPH() { return ph; };
    
    private:
        Manager* manInst;                                                       // Instance of the manager
        
        float ph;

       
};