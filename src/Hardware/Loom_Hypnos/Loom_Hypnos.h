#pragma once


#include <OPEnS_RTC.h>
#include <ArduinoLowPower.h>
#include <map>

#include "Arduino.h"
#include "Module.h"

#include "Hardware/Loom_Hypnos/SDManager.h"
#include "Loom_Manager.h"

#define CUSTOM_TIME_TIMEOUT 20 // The timeout length in seconds

// Used to pass along the user defined interrupt callback
using InterruptCallbackFunction = void (*)();

/**
 * Tracks the hypnos version and matches the version with the correct chip select pin
 */ 
enum HYPNOS_VERSION{
    V3_2 = 10,
    V3_3 = 11,
    ADALOGGER = 4
};

/**
 * Time zone abbreviations that map to the hour offset from UTC
 */ 
enum TIME_ZONE{
    WAT = 1,
    AT = 2,
    AST = 4,
    EST = 5,
    CST = 6,
    MST = 7,
    PST = 8,
    AKST = 9,
    HST = 9,
    SST = 11,
    GMT = 0,
    BST = -1,
    CET = -1,
    EET = -2,
    EEST = -3,
    BRT = -3,
    ZP4 = -4,
    ZP5 = -5,
    ZP6 = -6,
    ZP7 = -7,
    AWST = -8,
    ACST = -9, // Half an hour off so its -9.5
    AEST = -10

};

/**
 * All in one driver for the Hypnos board. This allows users to use the Hypnos board in a more modularized manner not requiring all the Loom stuff.
 * 
 * @author Will Richards
 */ 
class Loom_Hypnos : public Module{
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
         * @param version The version of the Hypnos in use, this changes which pin is used as and SD chip select
         * @param timezone The current timezone the clock was set to
         * @param use_custom_time Use a specific time set by the user that is different than the compile time
         * @param useSD Whether or not SD card functionality should be enabled
         */ 
        Loom_Hypnos(Manager& man, HYPNOS_VERSION version, TIME_ZONE zone, bool use_custom_time = false, bool useSD = true);

        /**
         *  Cleanup any dynamically allocated pointers
         */ 
        ~Loom_Hypnos();

        /* Power Control Functionality */

        /**
         * Enable the Hypnos board
         * Sets the power rail pins to OUTPUT mode and then enables them
         * 
         * @param enable33 whether or not to enable the 3.3v rails
         * @param enable5 whether or not to enable the 5v and 12v rails
         */ 
        void enable(bool enable33 = true, bool emable5 = true);

        /**
         * Disables the Hypnos Board
         * Disables the Power Rails and sets the SPI pins to INPUT which effectively disables them
         */ 
        void disable();

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
        bool registerInterrupt(InterruptCallbackFunction isrFunc = nullptr, int interruptPin = 12, int triggerState = LOW);

        /**
         * Called when the user wants to wake the Hypnos back out of the sleep state
         * This detaches the interrupt AND re-enables the power rails
         */ 
        void wakeup();

        /**
         * Called when the user wants to reattach the interrupt handler to the RTC interrupt to collect subsequent interrupts
         * @param interruptPin Pin to reattach the interrupt to for RTC this doesn't need to be changed
         */ 
        bool reattachRTCInterrupt(int interruptPin = 12);

        /**
         * Set the next interrupt to be triggered at a set interval in the future
         * @param duration The time that will elapse before the next interrupt is triggered
         */ 
        void setInterruptDuration(const TimeSpan duration);

        /**
         * Drops the Feather M0 and Hypnos board into a low power sleep waiting for an interrupt to wake it up and pull it out of sleep
         * @param waitForSerial Whether or not we should wait for the user to open the serial monitor before continuing execution
         */ 
        void sleep(bool waitForSerial = false);

        /**
         * Get the current time from the RTC
         */ 
        DateTime getCurrentTime(); 

        /**
         * Set a custom time on startup for the RTC to use
        */
        bool set_custom_time();
    
        /**
         * Get a custom sleep interval specified in a file on the SD card
         * If there was an error reading the file the sleep interval will be set to 20 minutes
         * @param fileName Name of the file to pull information from
         * @return TimeSpan with the parsed data
         */ 
        TimeSpan getSleepIntervalFromSD(const char* fileName);

        /**
         * Get and set the timezone for the Hypnos from the SD card
         * @param fileName Name of the file to pull information from
         */ 
        void getTimeZoneFromSD(const char* fileName);

        /**
         * Read file from SD
         * @param fileName File to read from
         */ 
        char* readFile(const char* fileName) { return sdMan->readFile(fileName); };

        /**
         * Get the default SD card file name 
         */
        const char* getDefaultFilename(){ return sdMan->getDefaultFilename(); };

        /**
         * Get an instance of the SD manager, used for batch SD
         */ 
        SDManager* getSDManager() { return sdMan; };

        /**
         * Set an alternative name to log data to
         */ 
        void setLogName(const char* name) { sdMan->setLogName(name); };

    private:

        Manager* manInst = nullptr;                                                         // Instance of the manager
        SDManager* sdMan = nullptr;                                                         // SD Manager

        int sd_chip_select;                                                                 // Pin that the SD card will use to communicate with the Hypnos
        bool enableSD;                                                                      // Specifies whether or not the SD card should be enabled on the Hypnos

        int batch_size;

        /* Real-Time Clock Settings */

        RTC_DS3231 RTC_DS;                                                                  // Real time clock reference
        bool RTC_initialized = false;                                                       // Did the RTC initialize correctly?
        
        bool custom_time = false;                                                           // Set the RTC to a user specified time
        InterruptCallbackFunction callbackFunc = nullptr;                                   // ISR call back for the RTC interrupt

        
        void initializeRTC();                                                               // Initialize RTC

        void createTimezoneMap();                                                           // Map Timezone Strings to Timezone enum
        std::map<const char*, TIME_ZONE> timezoneMap;                                       // String to Timezone enum

        DateTime get_utc_time();                                                            // Convert the local time to UTC, accounts for daylight savings zones
        TIME_ZONE timezone;                                                                 // Timezone the RTC was set to
       
        void dateTime_toString(DateTime time, char array[21]);                              // Convert a DateTime object to our desired format

        DateTime time;                                                                      // UTC time
        DateTime localTime;                                                                 // Local time

        /* Sleep functionality */

        void pre_sleep();                                                                   // Called just before the hypnos enters sleep, this disconnects the power rails and the serial bus
        void post_sleep(bool waitForSerial);                                                // Called just after the hypnos wakes up, this reconnects the power rails and the serial bus

        

};