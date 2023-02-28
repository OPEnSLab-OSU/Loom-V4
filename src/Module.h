#pragma once
#include "Arduino.h"
#include <Wire.h>
#include <stdio.h>
#include <string.h>
#include <ArduinoJson.h>
#include <Adafruit_SleepyDog.h>

/* Watchdog Timer Setup */
#define WATCHDOG_TIMEOUT 8000

// Only allow the Timer to be used if WATCHDOG_ENABLE is set
#if defined(WATCHDOG_ENABLE)
    #define TIMER_ENABLE Wathchdog.enable(WATCHDOG_TIMEOUT)
    #define TIMER_DISABLE Wathchdog.disable()
    #define TIMER_RESET Wathchdog.reset()
#else
    #define TIMER_ENABLE 
    #define TIMER_DISABLE
    #define TIMER_RESET
#endif

/**
 *  General overarching interface to provide basic unified functionality
 * 
 *  @author Will Richards
 */ 
class Module{
    public:
        Module(const char* modName) { strcpy(moduleName, modName); };

        void setModuleName(const char* modName) { strcpy(moduleName, modName); };

        virtual const char* getModuleName() { return moduleName; }; // Return the name of the sensor
        virtual void printModuleName(const char* message) { 
            char output[256];
            sprintf(output, "[%s] %s", getModuleName(), message)
            Serial.println(output);
        };

        // Generic measure and package calls to unify some interaction with different sensor implementations
        virtual void initialize() = 0;                      // Initialize all functionality of the sensor
        virtual void measure() = 0;                         // Collect data from the sensor
        virtual void package() = 0;                         // Package collected data into JSON document
        virtual void power_up() = 0;                        // Power the sensor up and come out of sleep
        virtual void power_down() = 0;                      // Power the sensor down to prepare for sleep

        // Not required overrides
        virtual void display_data() {};                     // Called by the manager to allow OLED to display data at the same time as manager.display_data  

        bool moduleInitialized = true;                      // Whether or not the module initialized successfully true until set otherwise
        int module_address = -1;                            // Specifically for I2C addresses, -1 means the module doesn't have an address
    private:
        char moduleName[100];
        
};