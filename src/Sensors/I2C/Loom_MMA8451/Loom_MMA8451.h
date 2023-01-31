#pragma once

#include <Wire.h>
#include <Adafruit_MMA8451.h>

#include "../I2CDevice.h"
#include "Loom_Manager.h"

// Consult the datasheet for MMA8451 to change these values
#define CTRL_REG3 0b11000000
#define CTRL_REG4 0b00100001
#define CTRL_REG5 0b00100000
#define REG_TRANS_CFG 0b00001110
#define REG_TRANS_CT 0b00000000

// Register Addresses
#define MMA8451_REG_CTRL_REG3 0x2C 
#define MMA8451_REG_TRANSIENT_CFG 0x1D
#define MMA8451_REG_TRANSIENT_THS 0x1F
#define MMA8451_REG_TRANSIENT_CT  0x20
#define MMA8451_REG_TRANSIENT_SRC 0x1E

// Used to pass along the user defined interrupt callback
using InterruptCallbackFunction = void (*)();

/**
 * MB1232 Distance sensor
 * 
 * @author Will Richards
 */ 
class Loom_MMA8451 : public I2CDevice{
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
                    mma8451_range_t range = MMA8451_RANGE_2_G,
                    int interruptPin = -1,
                    uint8_t sensitivity = 0x10
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

        /**
         * Set the ISR to be triggered when the interrupt is triggered
        */
        void setISR(InterruptCallbackFunction isr) { this->isr = isr; };

        static void IMU_ISR();



    private:
        Manager* manInst;                       // Instance of the manager
        Adafruit_MMA8451 mma;                   // Instance of the MMA sensor
        
        mma8451_range_t range;                  // Range of the sensor
        int address;                            // I2C address
        uint8_t sensitivity = 0x10;             // Sensitivity of detection

        static uint8_t interruptPin;            // Interrupt pin on movement
        static InterruptCallbackFunction isr;          // ISR to call when the interrupt is triggered

        float			accel[3];		        // Acceleration values for each axis. Units: g
	    uint8_t			orientation;	        // Orientation
};