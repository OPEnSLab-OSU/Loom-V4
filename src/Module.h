#pragma once

#include "Loom_WarningGuards.h"

/*
 * Module.h is included by almost every Loom class. Keep it narrow and treat
 * Arduino core headers as external so their compatibility stubs do not repeat
 * through the whole library under `--warnings all`.
 */
LOOM_EXTERNAL_INCLUDE_BEGIN
#include "Arduino.h"
#include <Adafruit_SleepyDog.h>
LOOM_EXTERNAL_INCLUDE_END

#include <stdio.h>
#include <string.h>

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

#define OUTPUT_SIZE 256
#define MAX_JSON_SIZE 2000

/**
 *  General overarching interface to provide basic unified functionality
 * 
 *  @author Will Richards
 */ 
class Module{
    public:
        Module(const char* modName) { strcpy(moduleName, modName); };
        virtual ~Module() {};

        void setModuleName(const char* modName) { strcpy(moduleName, modName); };

        virtual const char* getModuleName() { return moduleName; }; // Return the name of the sensor
        virtual void printModuleName(const char* message) {
            Serial.print("[");
            Serial.print(getModuleName());
            Serial.print("] ");
            Serial.println(message);
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
