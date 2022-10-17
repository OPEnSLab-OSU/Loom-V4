#pragma once

#include <Wire.h>
#include <Adafruit_MMA8451.h>

#include "Module.h"
#include "Loom_Manager.h"

/**
 * MB1232 Distance sensor
 * 
 * @author Will Richards
 */ 
class Loom_MMA8451 : public Module{
    protected:
        void power_down() override {}; 
        void print_measurements() override {};
       
        // Manager controlled functions
        void measure() override;                               
        void initialize() override;    
        void package() override;  
        void power_up() override; 

    public:
        /**
         * Constructs a new TSL2591 sensor
         * @param man Reference to the manager that is used to universally package all data
         * @param address I2C address that is assigned to the sensor
         * @param range Range of the MMA sensor
         */ 
        Loom_MMA8451(
                    Manager& man,
                    int addr = 0x1D,
                    bool useMux = false, 
                    mma8451_range_t range = MMA8451_RANGE_2_G
                );

        /**
         * Get the X Acceleration
         */ 
        float getAccelX() { return accel[0]; };

        /**
         * Get the Y Acceleration
         */ 
        float getAccelY() { return accel[1]; };

        /**
         * Get the Z Acceleration
         */ 
        float getAccelZ() { return accel[2]; };

        /**
         * Get the orientation
         */ 
        uint8_t getOrientation() { return orientation; };

        



    private:
        Manager* manInst;                       // Instance of the manager
        Adafruit_MMA8451 mma;                   // Instance of the MMA sensor
        
        mma8451_range_t range;                  // Range of the sensor
        int address;                            // I2C address

        float			accel[3];		        // Acceleration values for each axis. Units: g
	    uint8_t			orientation;	        // Orientation
};