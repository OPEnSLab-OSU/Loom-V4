#pragma once
#include "Arduino.h"
#include <ArduinoJson.h>

/**
 *  General overarching interface to provide basic unified functionality
 */ 

class Module{
    public:
        Module(String modName) {moduleName = modName;};
        String getModuleName() { return moduleName; }; // Return the name of the sensor
        void printModuleName() { Serial.print("[" + String(getModuleName()) + "] "); };

        // Generic measure and package calls to unify some interaction with different sensor implementations
        virtual void initialize() = 0;
        virtual void measure() = 0;
        virtual void package() = 0;
        virtual void print_measurements() = 0;
        virtual void power_up() = 0;
        virtual void power_down() = 0;

        bool moduleInitialized = true;
    private:
        String moduleName;
        
};