#pragma once

#include <ZX_Sensor.h>

#include "Module.h"
#include "Loom_Manager.h"

/**
 * ZX Gesture distance sensor
 * 
 * @author Will Richards
 */ 
class Loom_ZXGesture : public Module{
    protected:

        void power_up() override {};
        void power_down() override {}; 
        void print_measurements() override {};
       
        // Manager controlled functions
        void measure() override;                               
        void initialize() override;    
        void package() override;   

    public:

        /**
         * Struct for representing position information
         */ 
        struct Position{
            uint8_t x;
            uint8_t z;
        };

        /**
         * Mode to put the sensor into which alters what type of data it is trying to sense
         */ 
        enum Mode {
            POS,
            GEST
        };

        /**
         * Constructs a new TSL2591 sensor
         * @param man Reference to the manager that is used to universally package all data
         * @param address I2C address that is assigned to the sensor
         * @param mode Measuring mode we want to interpret data using
         */ 
        Loom_ZXGesture(
                      Manager& man, 
                      int address = 0x10, 
                      Mode mode = Mode::POS
                );

        /**
         * Get last recorded gesture
         */ 
        String getGesture() {return gestureString; };

        /**
         * Get the speed at which the last gesture has been preformed
         */ 
        uint8_t getGestureSpeed() { return gestureSpeed; };

        /**
         * Get position in mm
         */ 
        Position getPosition() { return pos; };



    private:
        Manager* manInst;                       // Instance of the manager
        ZX_Sensor zx;                           // ZX sensor instance

        Mode mode;                              // Current mode of the sensor 
        Position pos;                           // Position measured by the sensor (X and Y) in mm

        GestureType gesture;                    // Last measured gesture
        String gestureString;                   // String name of the last gesture
        uint8_t gestureSpeed;                   // The speed at which the gesture was preformed
};