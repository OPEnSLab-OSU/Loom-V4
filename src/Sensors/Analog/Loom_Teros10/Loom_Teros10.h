#pragma once

#include <Adafruit_ADS1X15.h>
#include <cmath>

#include "Module.h"
#include "Loom_Manager.h"

/**
 * Functionality for the Teros 10
 * 
 * @author Will Richards
 */  
class Loom_Teros10 : public Module{


    protected:
        void initialize() override {};
        void print_measurements() override {};  
        void power_up() override {};
        void power_down() override {}; 

    public:
        
        void measure() override;
        void package() override;

        /**
         *  Construct a new Teros 10
         *  @param man Reference to the manager
         *  @param port Which analog port to read from
         */ 
        Loom_Teros10(
                Manager& man,
                int port = A0 
            );


        /**
         * Get the Volumetric Water Content
         */ 
        float getVWC() { return volumetricWater; };

        /**
         * Get the dielectric permittivity
         */ 
        float getDialecPerm() { return dielecPerm; };
    
    private:
        Manager* manInst;                                           // Instance of the manager
        int analogPort = A0;                                        // Where the analog sensor is hooked up 
        
        float volumetricWater;                                      // Volumetric water content
        float dielecPerm;                                           // Dielectric permittivity

        float analogToMV(int analog);                               // Convert the analog voltage to mV
        float computeVWC(float mV);                                 // Calculate the Volumetric Water Content from the mV
        float computeDP(float mV);                                  // Calculate the Dielectric Permittivity from the mV
};