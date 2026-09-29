#pragma once

#include "Loom_WarningGuards.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include "Arduino.h"
#include <Adafruit_SleepyDog.h>
LOOM_EXTERNAL_INCLUDE_END
#include <stdio.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
// Watchdog: restart the board if a protected operation stops making progress.
////////////////////////////////////////////////////////////////////////////////////////////////////
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
    const bool watchdogEnabled = WDT->CTRLA.bit.ENABLE;
#else
    const bool watchdogEnabled = WDT->CTRL.bit.ENABLE;
#endif
    if (watchdogEnabled) {
        Watchdog.reset();
    }
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
 * One part of a Loom device: a sensor, display, storage module, or connection.
 * Manager calls these same operations on each registered module in registration order.
 *
 *  @author Will Richards
 */
class Module {
  public:
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Module name: the label used in logs and the packet.
    ////////////////////////////////////////////////////////////////////////////////////////////////
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

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Measurement cycle: setup -> read -> package; power down/up surrounds sleep.
    ////////////////////////////////////////////////////////////////////////////////////////////////
    virtual void initialize() = 0; // Set up the hardware before the first reading.
    virtual void measure() = 0;    // Read the hardware and keep the latest values in this module.
    virtual void package() = 0;    // Add those values to Manager's shared JSON packet.
    virtual void power_up() = 0;   // Restore the hardware after sleep.
    virtual void power_down() = 0; // Prepare the hardware for sleep.
    // A failed module normally stays skipped; some connections can recover on the next wake.
    virtual bool retryPowerUpWhenUninitialized() const { return false; }

    // Not required overrides
    virtual void display_data() {}; // Called by the manager to allow OLED to display data at the
                                    // same time as manager.display_data

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Availability: used by Manager to decide whether to call this module.
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Drivers clear this flag when setup or recovery fails. Manager skips unavailable modules.
    bool moduleInitialized = true;
    int module_address = -1; // I2C address, or -1 when the module has no I2C address.
  private:
    char moduleName[MODULE_NAME_SIZE];
};
