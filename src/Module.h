#pragma once

#include "Arduino.h"
#include <Adafruit_SleepyDog.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <stdio.h>
#include <string.h>

/* Watchdog Timer Setup */
#define WATCHDOG_TIMEOUT 10000

// To allow Watchdog functionality, WATCHDOG_ENABLE must be set during compilation
#if defined(WATCHDOG_ENABLE)
#define WD_TIMER_ENABLE Watchdog.enable(WATCHDOG_TIMEOUT)
#define WD_TIMER_DISABLE Watchdog.disable()
#define WD_TIMER_RESET Watchdog.reset()
#else
#define WD_TIMER_ENABLE
#define WD_TIMER_DISABLE
#define WD_TIMER_RESET
#endif

// Field sketches may enable SleepyDog at runtime rather than defining WATCHDOG_ENABLE for every
// Loom translation unit. Long sensor averaging loops use this helper so a healthy SAMD21 sample
// can exceed one watchdog period without hiding a genuinely stuck I2C transaction.
inline void loomResetWatchdogIfEnabled() {
#if defined(ARDUINO_ARCH_SAMD)
#if defined(__SAMD51__)
    if (WDT->CTRLA.bit.ENABLE)
#else
    if (WDT->CTRL.bit.ENABLE)
#endif
        Watchdog.reset();
#elif defined(WATCHDOG_ENABLE)
    Watchdog.reset();
#endif
}

#ifndef TIMER_ENABLE
#define TIMER_ENABLE WD_TIMER_ENABLE
#define TIMER_DISABLE WD_TIMER_DISABLE
#define TIMER_RESET WD_TIMER_RESET
#endif

#define OUTPUT_SIZE 256
#define MAX_JSON_SIZE 2000
// Built-in module names are at most 18 characters; mux/address suffixes still fit comfortably.
#define MODULE_NAME_SIZE 32

/**
 *  General overarching interface to provide basic unified functionality
 *
 *  @author Will Richards
 */
class Module {
  public:
    Module(const char *modName) { setModuleName(modName); };
    virtual ~Module() = default;

    void setModuleName(const char *modName) {
        strncpy(moduleName, modName ? modName : "", sizeof(moduleName) - 1);
        moduleName[sizeof(moduleName) - 1] = '\0';
    };

    virtual const char *getModuleName() { return moduleName; }; // Return the name of the sensor
    virtual void printModuleName(const char *message) {
        Serial.print('[');
        Serial.print(getModuleName());
        Serial.print(F("] "));
        Serial.println(message ? message : "");
    };

    // Generic measure and package calls to unify some interaction with different sensor
    // implementations
    virtual void initialize() = 0; // Initialize all functionality of the sensor
    virtual void measure() = 0;    // Collect data from the sensor
    virtual void package() = 0;    // Package collected data into JSON document
    virtual void power_up() = 0;   // Power the sensor up and come out of sleep
    virtual void power_down() = 0; // Power the sensor down to prepare for sleep
    virtual bool retryPowerUpWhenUninitialized() const { return false; }

    // Not required overrides
    virtual void display_data() {}; // Called by the manager to allow OLED to display data at the
                                    // same time as manager.display_data

    bool moduleInitialized =
        true; // Whether or not the module initialized successfully true until set otherwise
    int module_address =
        -1; // Specifically for I2C addresses, -1 means the module doesn't have an address
  private:
    char moduleName[MODULE_NAME_SIZE];
};
