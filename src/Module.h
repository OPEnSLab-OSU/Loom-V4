#pragma once

#include "Loom_WarningGuards.h"
#include "Utilities/Loom_Watchdog.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include "Arduino.h"
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
#define WD_TIMER_RESET LOOM_FEED_WATCHDOG()
#else
#define WD_TIMER_ENABLE
#define WD_TIMER_DISABLE
#define WD_TIMER_RESET
#endif

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
    // Optional startup settling before anchoring a sampling schedule. No packet is produced.
    virtual void prepareForSampling() {}
    // A failed module normally stays skipped; some connections can recover on the next wake.
    virtual bool retryPowerUpWhenUninitialized() const { return false; }
    // Opt in only when package() can safely report unavailable values without hardware I/O.
    // This keeps intermittently powered connections from changing the CSV column layout.
    virtual bool packageWhenUnavailable() const { return false; }

    // Idle keeps Hypnos rails on. A driver may stop its fan/conversions here and restart them
    // in resume(); the default leaves hardware alone, so older drivers remain compatible.
    virtual void idle() {}
    virtual void resume() {}
    // A connection can refuse rail removal after an unacknowledged graceful shutdown.
    virtual bool canRemovePower() const { return true; }

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
