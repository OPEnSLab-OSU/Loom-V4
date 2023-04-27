#pragma once

#include <cmath>
#include <deque>

#include "Module.h"
#include "Loom_Manager.h"

using TipCallBack = void (*)();


/**
 * Functionality for interrupt based tipping buckets
 * 
 * @author Will Richards
 */  
class Loom_TippingBucket : public Module{
    protected:


        void power_up() override {};
        void power_down() override {}; 

    public:
        void initialize() override {};

        void measure() override;
        void package() override;

        void set 

        /**
         *  Construct a new Tipping Bucket instance
         *  @param man Reference to the manager
         *  @param port Which port to expect the interrupt
         */ 
        Loom_TippingBucket(
                Manager& man,
                int pin = A0,
            );


        /**
         * Get the total tip count
         */ 
        int getCount() { return count; };

        /* Increment the total number of counts*/
        void incrementCount() { count++; };

        /**
         * Get the dielectric permittivity
         */ 
        float getDialecPerm() { return dielecPerm; };
    
    private:
        Manager* manInst;                                           // Instance of the manager
        int analogPort = A0;                                        // Where the analog sensor is hooked up 
        
        int count = 0;                                              // Total number of tips
        std::deque<int> last4Counts;                                // Track the last four 
        bool hasTipped = false;                             
        volatile unsigned long last_micros;

        float analogToMV(int analog);                               // Convert the analog voltage to mV
        float computeVWC(float mV);                                 // Calculate the Volumetric Water Content from the mV
        float computeDP(float mV);                                  // Calculate the Dielectric Permittivity from the mV
};