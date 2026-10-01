#pragma once

#include "Loom_WarningGuards.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <OPEnS_RTC.h>
LOOM_EXTERNAL_INCLUDE_END

#if !defined(LOOM_OPENS_RTC_PATCH_LEVEL) || LOOM_OPENS_RTC_PATCH_LEVEL < 1
#error "Loom_Hypnos requires the hardened OPEnS_RTC dependency from Loom/dependencies."
#endif

LOOM_EXTERNAL_INCLUDE_BEGIN
#include "Arduino.h"
LOOM_EXTERNAL_INCLUDE_END
#include "Module.h"

#include "Hardware/Loom_Hypnos/SDManager.h"
#include "Loom_Manager.h"
#include "Sensors/Loom_Analog/Loom_Analog_Config.h"

class NetworkComponent;

// Used to pass along the user defined interrupt callback
using InterruptCallbackFunction = void (*)();

/**
 * Enum to represent all power rail configurations
 */
enum POWERRAIL_CONFIG {
    PR_3V_ON_5V_ON,  // Both the 3v and 5v rails are enabled
    PR_3V_ON_5V_OFF, // The 3v rail is enabled and the 5v rail is disabled
    PR_3V_OFF_5V_ON, // The 3v rail is disabled and the 5v rail is enabled
    PR_3V_OFF_5V_OFF // The 3v rail and the 5v rail are both disabled
};

/**
 * Enum to easily see if we are going to sleep or waking up from sleep
 */
enum DEVICE_STATE { ENTERING_SLEEP, EXITING_SLEEP };

/**
 * Tracks the hypnos version and matches the version with the correct chip select pin
 */
enum HYPNOS_VERSION { V3_2 = 10, V3_3 = 11, ADALOGGER = 4 };

/**
 * Time zone abbreviations. Most values are their whole-hour standard UTC offset; ACST is handled
 * as UTC+09:30 by the conversion code.
 */
enum TIME_ZONE {
    WAT = -1,
    AT = -2,
    AST = -4,
    EST = -5,
    CST = -6,
    MST = -7,
    PST = -8,
    AKST = -9,
    HST = -10,
    SST = -11,
    GMT = 0,
    BST = 1,
    CET = 1,
    EET = 2,
    EEST = 3,
    BRT = 3,
    ZP4 = 4,
    ZP5 = 5,
    ZP6 = 6,
    ZP7 = 7,
    AWST = 8,
    ACST = 9, // Australian Central Standard Time is UTC+09:30
    AEST = 10

};

/**
 * Type of interrupt to register
 */
enum HypnosInterruptType : uint8_t { SLEEP, OTHER };

namespace loomPower {
class RechargePolicy;
}

/**
 * All in one driver for the Hypnos board. This allows users to use the Hypnos board in a more
 * modularized manner not requiring all the Loom stuff.
 *
 * @author Will Richards
 */
class Loom_Hypnos : public Module {
  protected:
    /* These aren't used with the Hypnos */
    void measure() override {};

    void initialize() override {};

    void power_up() override {};
    void power_down() override {};

    // We want to use the package method to add the timestamp to the JSON
    void package() override;

  public:
    volatile bool shouldPowerUp = true;

    /**
     * Constructs a new Hypnos Instance using the manager to hold information about the device
     * @param man Reference to the manager
     * @param version The version of the Hypnos in use, this changes which pin is used as and SD
     * chip select
     * @param timezone The current timezone the clock was set to
     * @param use_custom_time Use a specific time set by the user that is different than the compile
     * time
     * @param useSD Whether or not SD card functionality should be enabled
     */
    Loom_Hypnos(Manager &man, HYPNOS_VERSION version, TIME_ZONE zone, bool use_custom_time = true,
                bool useSD = true);

    /**
     *  Cleanup any dynamically allocated pointers
     */
    ~Loom_Hypnos();

    Loom_Hypnos(const Loom_Hypnos &) = delete;
    Loom_Hypnos &operator=(const Loom_Hypnos &) = delete;

    /* Power Control Functionality */

    /**
     * Enable the Hypnos board
     * Sets the power rail pins to OUTPUT mode and then enables them
     *
     * @param enable33 whether or not to enable the 3.3v rails
     * @param enable5 whether or not to enable the 5v and 12v rails
     */
    void enable();
    void enable(bool enable33, bool enable5 = true);

    /**
     * Apply the configured wake power-rail state immediately.
     */
    void applyWakeConfiguration();

    /**
     * Disables the Hypnos Board
     * Disables the Power Rails and sets the SPI pins to INPUT which effectively disables them
     */
    void disable(bool disable33 = true, bool disable5 = true);

    /**
     * Set the configuration for the power rails when going to sleep
     * @param config The configuration that we wish to perform when entering sleep
     */
    void setSleepConfiguration(POWERRAIL_CONFIG config) { sleepModePowerConfig = config; };

    /**
     * Set and immediately apply the power rail configuration used after waking.
     * @param config The configuration to apply and retain for wake-up
     */
    void setWakeConfiguration(POWERRAIL_CONFIG config);
    /** Read configured rails for diagnostics; these values do not measure rail voltage. */
    POWERRAIL_CONFIG getWakeConfiguration() const { return wakeModePowerConfig; }
    POWERRAIL_CONFIG getSleepConfiguration() const { return sleepModePowerConfig; }

    /* SD Functionality */

    /**
     * Log the current sensor data to a file on the SD card
     */
    bool logToSD();

    /* Sleep Functionality */

    /**
     * Enables RTC based interrupts using the DS3231 on the Hypnos
     * @param isrFunc function to callback to when the interrupt is triggered
     * @param interruptPin Defaults to RTC pin on Hypnos can be changed to reflect other interrupts
     * @param interruptType Type of the interrupt to register (SLEEP or OTHER)
     * @param triggerState When the interrupt should trigger
     */
    bool registerInterrupt(InterruptCallbackFunction isrFunc = nullptr, int interruptPin = 12,
                           HypnosInterruptType interruptType = SLEEP, int triggerState = LOW);

    /**
     * Called when the user wants to wake the Hypnos back out of the sleep state
     * This detaches the interrupt AND re-enables the power rails
     */
    void wakeup();

    /**
     * Called when the user wants to reattach the interrupt handler to the RTC interrupt to collect
     * subsequent interrupts
     * @param interruptPin Pin to reattach the interrupt to for RTC this doesn't need to be changed
     */
    bool reattachRTCInterrupt(int interruptPin = 12);

    /**
     * Set the next interrupt to be triggered at a set interval in the future
     * @param duration The time before the next interrupt, from 1 second through 27 days
     */
    void setInterruptDuration(const TimeSpan duration);

    /**
     * Schedule a relative RTC wake and return whether its alarm was verified successfully.
     * Call sleep() separately. Use this when acknowledging a remote command or deciding
     * whether it is safe to enter standby; the legacy void setter remains available.
     */
    bool scheduleWake(const TimeSpan duration);

    /**
     * Keep sample starts on a fixed UTC interval, including time spent measuring/uploading.
     * Call at the TOP of loop(), before measuring, then call sleep() after logging. The first
     * call anchors the schedule; later calls keep that phase and skip missed slots. A changed
     * interval or an observed backwards RTC reading starts a new phase. Local/DST display is
     * separate. Valid intervals are 1 second through 27 days; allow enough time for the actual
     * work and sleep preparation, or missed slots will be skipped.
     * Returns false if the alarm cannot be safely armed; sleep() then remains awake.
     * Use setInterruptDuration() instead when you deliberately want a full rest AFTER work.
     */
    bool setSampleInterval(const TimeSpan interval);
    /** On an interval change, true anchors the new period to the last due wake instead of
     * adding active work time. A future old alarm or backwards RTC correction starts fresh.
     * Useful when an SD-controlled stress test changes its interval after a verified wake. */
    bool setSampleInterval(const TimeSpan interval, bool keepLastWakeAnchor);

    /** Read the cached, verified UTC wake deadline without another RTC transaction. */
    bool getScheduledWakeTime(DateTime &utc) const {
        if (!alarmScheduled) {
            return false;
        }
        utc = alarmTime;
        return true;
    }

    /**
     * Drops the Feather M0 and Hypnos board into a low power sleep waiting for an interrupt to wake
     * it up and pull it out of sleep
     * @param waitForSerial Whether or not we should wait for the user to open the serial monitor
     * before continuing execution
     */
    void sleep(bool waitForSerial = false);

    /**
     * Gracefully shut down modules and sleep until the next battery check. Both sensor rails
     * are switched off during standby. On return the RTC/SD/USB path is restored briefly,
     * then sensor rails remain off and Manager does NOT restart LTE or sensor modules.
     * Call enable() followed by Manager::power_up() only after voltage has recovered.
     * Returns false if no safe RTC standby was entered. Requires the standard pin-12 RTC wake.
     */
    bool sleepForRecharge(const TimeSpan checkInterval = TimeSpan(3600));
    /** Optional processor RTC wake instead of Hypnos RTC. Owns the ArduinoLowPower internal
     *
     * RTC alarm; do not combine with another RTCZero owner. Sensor rails remain off. */
    bool sleepForRechargeInternal(const TimeSpan checkInterval = TimeSpan(3600));

    /**
     * Opt in to a battery check BEFORE modules restart after normal sleep. Policy/reader must
     * remain alive for this Hypnos instance; a null reader or unconfigured policy is rejected.
     * The sketch must also call sleepForRecharge() while its policy requests charging.
     */
    bool setRechargePolicy(loomPower::RechargePolicy &policy, float (*readBatteryVolts)());

    /** Opt-in runtime protection of wake reinitialization; 0 preserves legacy behavior.
     * Hypnos suspends this guard across standby and restores it before wake I/O.
     */
    void setWakeWatchdogTimeout(uint16_t milliseconds) { wakeWatchdogMs = milliseconds; }

    /**
     * Get the current time from the RTC
     */
    DateTime getCurrentTime();
    // Checked UTC read: rejects oscillator-stop status, I2C faults and invalid calendar fields.
    // Failure leaves the caller's time unchanged; use this for logging/scheduling/metadata.
    bool tryGetCurrentTime(DateTime &utc);

    /**
     * Convert the current time to a ISO 8601 compatible time string
     *
     * @param time The current time as a DateTime object
     * @param array The buffer to write the string to (size 21)
     */
    void dateTime_toString(DateTime time, char array[21], bool isLocal = false);

    /**
     * Set a custom time on startup for the RTC to use
     */
    void set_custom_time();

    /** Supply sketch-level __DATE__/__TIME__ before enable(). */
    void setCompileTime(const char *buildDate, const char *buildTime);

    /**
     * Load the configuration for the hypnos from the SD card (Timezeone and sleep interval)
     * @param fileName The file name on the root of the SD card to retrieve the information from
     * @return Return the time span for which the device is intended to sleep for
     */
    TimeSpan getConfigFromSD(const char *fileName);

    /**
     * Read file from SD
     * @param fileName File to read from
     */
    char *readFile(const char *fileName) { return sdMan ? sdMan->readFile(fileName) : nullptr; };

    /**
     * Get the default SD card file name
     */
    const char *getDefaultFilename() { return sdMan ? sdMan->getDefaultFilename() : ""; };

    /**
     * Get an instance of the SD manager, used for batch SD
     */
    SDManager *getSDManager() { return sdMan; };

    /**
     * Restart the MCU with an explicit reason (1..63 bytes). With SD enabled, first save intent
     * durably; return false and stay awake if that fails. A successful request never returns.
     * Call from ordinary sketch code, never an interrupt handler. Routine watchdog feeds do
     * not use this method. No existing Loom path requests an automatic software reset.
     */
    bool requestReset(const char *reason);

    /* Set a network interface in the Hypnos so we can sync our time */
    void setNetworkInterface(NetworkComponent *component) { networkComponent = component; };

    /* Set the current RTC time to the time retrieved from the network */
    bool networkTimeUpdate();

    /* Whether or not the configured timezone is currently observing daylight saving time. */
    bool isDaylightSavings();

    /**
     * Evaluate the current North American DST rule for a specific UTC timestamp.
     * DST begins at 02:00 local standard time on the second Sunday in March and ends at 02:00
     * local daylight time on the first Sunday in November.
     */
    static bool isDaylightSavingsForDate(const DateTime &utcTime, TIME_ZONE zone);

    /** Return the requested weekday occurrence in a month. dow uses the RTC's 0=Sunday convention.
     * A week value of zero returns the final occurrence in the month.
     */
    static DateTime nthWeekdayOfMonth(int year, int month, int dow, int week, int hour);

    /**
     * Set an alternative name to log data to
     */
    void setLogName(const char *name) {
        if (sdMan != nullptr) {
            sdMan->setLogName(name);
        } else {
            printModuleName("Cannot select an SD log name because SD support is disabled.");
        }
    };

    /* Return initialization state of the RTC */
    bool isRTCInitialized() { return RTC_initialized; };

    /**
     * @brief A minimum required voltage check for a complete cycle. Minimum voltage varies by
     * device and should be determined by the user. This method is more flexible than the analog
     * class and allows us to create control methods in the hypnos class based on the voltage.
     *
     * @param voltage_min Minimum required voltage for your device (default = 0.0)
     * @param analogPin Any pin connected to the ADC input channel. (default = A7)
     * @param scale Resistance divisor for +VIN --> VOUT (default = 2.0 for A7)
     * @param mv Whether you want millivolts returned with volts. (default = false)
     * @param num_samples Number of samples if you want to get an average. (default = 1)
     */
    bool checkVoltage(float vmin = 0.0f, int analogPin = LOOM_ANALOG_BATTERY_PIN,
                      float scale = LOOM_ANALOG_BATTERY_DIVIDER_SCALE, bool mv = false,
                      int num_samples = 1);

  private:
    Manager *manInst = nullptr;                   // Instance of the manager
    NetworkComponent *networkComponent = nullptr; // Reference to a NetworkComponent

    /* Power rail setup */
    // Power rail configuration for when the device is awake
    POWERRAIL_CONFIG wakeModePowerConfig = PR_3V_ON_5V_ON;

    // Power rail configuration for the when the device is asleep
    POWERRAIL_CONFIG sleepModePowerConfig = PR_3V_OFF_5V_OFF;

    /**
     * Based on the state we are entering determine the configuration of the 3V power rail
     * @param state The new state the device is entering
     */
    bool is3VDisabled(DEVICE_STATE deviceState);

    /**
     * Based on the state we are entering determine the configuration of the 5V power rail
     * @param state The new state the device is entering
     */
    bool is5VDisabled(DEVICE_STATE deviceState);

    void setPowerRails(bool enable33, bool enable5);

    /* SD configuration */
    SDManager *sdMan = nullptr; // SD Manager
    uint16_t wakeWatchdogMs = 0;
    loomPower::RechargePolicy *rechargePolicy = nullptr; // Borrowed, configured by the sketch.
    float (*readBatteryVolts)() = nullptr;
    void enableWakeWatchdog();
    int sd_chip_select; // Pin that the SD card will use to communicate with the Hypnos
    bool enableSD;      // Specifies whether or not the SD card should be enabled on the Hypnos

    int batch_size;

    /* Real-Time Clock Settings */

    RTC_DS3231 RTC_DS;               // Real time clock reference
    bool RTC_initialized = false;    // Did the RTC initialize correctly?
    bool rtcWriteUnverified = false; // A failed/partial time write blocks UTC until a verified set.

    bool custom_time = false; // Set the RTC to a user specified time
    char sketchCompileDate[12] = {};
    char sketchCompileTime[9] = {};

    /* Voltage check bitmaps 0-7 LSB-first */
    static constexpr uint8_t VF_CHECKED = (1u << 0);    // 00000001 | 0x01
    static constexpr uint8_t VF_CRITICAL = (1u << 1);   // 00000010 | 0x02
    static constexpr uint8_t VF_DEGRADED = (1u << 2);   // 00000100 | 0x04
    static constexpr uint8_t VF_ACCEPTABLE = (1u << 3); // 00001000 | 0x08
    static constexpr uint8_t VF_LTE_READY = (1u << 4);  // 00010000 | 0x10
    static constexpr uint8_t VF_OPTIMAL = (1u << 5);    // 00100000 | 0x20

    static constexpr float VREF = 3.3f; // Reference voltage based on the feather m0 3.3v rail

    /* Voltage check status voltages (subject to change but should be acceptable for all devices)*/
    static constexpr float V_CRITICAL = 3.0f; // Below this, device may not function
    static constexpr float V_DEGRADED = 3.2f; // Minimum for device operation
    static constexpr float V_ACCEPTABLE = 3.35f;
    static constexpr float V_LTE_MIN = 3.45f; // Minimum for LTE transmission
    static constexpr float V_OPTIMAL = 3.7f;

    uint8_t voltage_flags = 0; // Flag mask defaults to 0x00

    // Feather M0 deployments use one RTC wake source; retain room for a second source without
    // allocating std::map nodes on the 32 KB SAMD21 heap.
    struct InterruptRegistration {
        InterruptCallbackFunction callback = nullptr;
        int16_t pin = -1;
        int8_t triggerState = LOW;
        HypnosInterruptType type = OTHER;
    };
    static constexpr uint8_t MAX_INTERRUPT_REGISTRATIONS = 2;
    InterruptRegistration interruptRegistrations[MAX_INTERRUPT_REGISTRATIONS];
    InterruptRegistration *findInterruptRegistration(int pin);
    int sleepInterruptPin = -1;

    void initializeRTC();                  // Initialize RTC
    bool writeRtcUtc(const DateTime &utc); // One owner for write/readback and uncertain-time state.
    bool releaseRTCInterrupt();            // Consume a wake alarm and verify INT/SQW is deasserted.

    static void sdFileDateTime(uint16_t *date, uint16_t *time); // SdFat create/sync clock callback
    DateTime getLocalTime(DateTime time); // Convert a given UTC time to local time
    TIME_ZONE timezone;                   // Timezone the RTC was set to

    DateTime time;      // UTC time
    DateTime localTime; // Local time

    DateTime alarmTime;                 // Time the alarm has been set for
    uint32_t sampleIntervalSeconds = 0; // Zero selects legacy relative-delay alarms.
    uint32_t nextSampleUtc = 0;
    uint32_t lastSampleClockUtc = 0;
    bool armRTCAlarm(const DateTime target); // Shared checked DS3231 arm/readback sequence.
    bool alarmScheduled = false;

    /* Sleep functionality */
    bool sleepImpl(bool waitForSerial, bool wakeModules);
    bool restoreModulesAfterSleep(bool wakeModules); // Battery gate for normal and failed wakes.
    bool pre_sleep(bool forceRailsOff = false, bool externalWake = true);
    void post_sleep(bool waitForSerial,
                    bool wakeModules); // Called just after the hypnos wakes up, this reconnects
                                       // the power rails and the serial bus
};
