#pragma once
#include "Arduino.h"
#include <ArduinoJson.h>

/**
 *  General overarching interface to provide basic unified functionality
 * 
 *  @author Will Richards
 */ 
class Module{
    public:
        Module(String modName) {moduleName = modName;};

       
        void setModuleName(String moduleName) { this->moduleName = moduleName; };

        virtual String getModuleName() { return moduleName; }; // Return the name of the sensor
        virtual void printModuleName() { Serial.print("[" + String(getModuleName()) + "] "); };

        // Generic measure and package calls to unify some interaction with different sensor implementations
        virtual void initialize() = 0;                      // Initialize all functionality of the sensor
        virtual void measure() = 0;                         // Collect data from the sensor
        virtual void package() = 0;                         // Package collected data into JSON document
        virtual void print_measurements() = 0;              // Print the measurements from that sensor to the serial monitor
        virtual void power_up() = 0;                        // Power the sensor up and come out of sleep
        virtual void power_down() = 0;                      // Power the sensor down to prepare for sleep

        // Not required overrides
        virtual void display_data() {};                     // Called by the manager to allow OLED to display data at the same time as manager.display_data  

        bool moduleInitialized = true;
    private:
        String moduleName;
        
};