#pragma once

#include <OPEnS_RTC.h>
#include <LowPower.h>

#include "Arduino.h"
#include "Module.h"

#include "Hardware/Loom_Hypnos/SDManager.h"
#include "Loom_Manager.h"

// Used to pass along the user defined interrupt callback
using InterruptCallbackFunction = void (*)();

/**
 * Tracks the hypnos version and matches the version with the correct chip select pin
 */ 
enum HYPNOS_VERSION{
    V3_2 = 10,
    V3_3 = 11
};

/**
 * All in one driver for the Hypnos board. This allows users to use the Hypnos board in a more modularized manner not requiring all the Loom stuff.
 */ 
class Loom_Hypnos : public Module{
    protected:

        /* These aren't used with the Hypnos */
        void measure() override {};                               
        void print_measurements() override {};
        void initialize() override {};    
        void power_up() override {};
        void power_down() override {}; 

        // We want to use the package method to add the timestamp to the JSON
        void package() override;                           
    public:

        /**
         * Constructs a new Hypnos Instance
         * @param version The version of the Hypnos in use, this changes which pin is used as and SD chip select
         * @param use_custom_time Use a specific time set by the user that is different than the compile time
         * @param useSD Whether or not SD card functionality should be enabled
         */ 
        Loom_Hypnos(HYPNOS_VERSION version, bool use_custom_time = false, bool useSD = false);

        /**
         * Constructs a new Hypnos Instance using the manager to hold information about the device
         * @param man Reference to the manager
         * @param version The version of the Hypnos in use, this changes which pin is used as and SD chip select
         * @param use_custom_time Use a specific time set by the user that is different than the compile time
         * @param useSD Whether or not SD card functionality should be enabled
         */ 
        Loom_Hypnos(Manager& man, HYPNOS_VERSION version, bool use_custom_time = false, bool useSD = true);

        /**
         *  Cleanup any dynamically allocated pointers
         */ 
        ~Loom_Hypnos();

        /* Power Control Functionality */

        /**
         * Enable the Hypnos board
         * Sets the power rail pins to OUTPUT mode and then enables them
         */ 
        void enable();

        /**
         * Disables the Hypnos Board
         * Disables the Power Rails and sets the SPI pins to INPUT which effectively disables them
         */ 
        void disable();

        /* SD Functionality */

        /**
         * Log the current sensor data to a file on the SD card
         */ 
        bool logToSD() { sdMan->log(getCurrentTime()); };

        /* Sleep Functionality */

        /**
         * Enables RTC based interrupts using the DS3231 on the Hypnos
         * @param isrFunc function to callback to when the interrupt is triggered
         */ 
        bool registerInterrupt(InterruptCallbackFunction isrFunc = nullptr);

        /**
         * Called when the user wants to wake the Hypnos back out of the sleep state
         * This detaches the interrupt AND re-enables the power rails
         */ 
        void wakeup();

        /**
         * Called when the user wants to reattach the interrupt handler to the RTC interrupt to collect subsequent interrupts
         */ 
        bool reattachRTCInterrupt();

        /**
         * Set the next interrupt to be triggered at a set interval in the future
         * @param duration The time that will elapse before the next interrupt is triggered
         */ 
        void setInterruptDuration(const TimeSpan duration);

        /**
         * Drops the Feather M0 and Hypnos board into a low power sleep waiting for an interrupt to wake it up and pull it out of sleep
         * @param waitForSerial whether or not we should wait for the user to open the serial monitor before continuing execution
         */ 
        void sleep(bool waitForSerial = false);

        /**
         * Get the current time from the RTC
         */ 
        DateTime getCurrentTime() {
            if(RTC_initialized)
                return RTC_DS.now(); 
            else{
                printModuleName(); Serial.println("Attempted to pull time when RTC was not previously initialized! Returned default datetime");
                return DateTime();
            }
        };
    
    private:

        Manager* manInst = nullptr;                                         // Instance of the manager
        SDManager* sdMan = nullptr;                                         // SD Manager

        int sd_chip_select;                                                 // Pin that the SD card will use to communicate with the Hypnos
        bool enableSD;                                                      // Specifies whether or not the SD card should be enabled on the Hypnos

        /* Real-Time Clock Settings */

        RTC_DS3231 RTC_DS;                                                  // Real time clock reference
        bool RTC_initialized = false;                                       // Did the RTC initialize correctly?
        
        bool custom_time = false;

        bool hasInterruptBeenRegistered = false;                            // If we have actually registered and interrupt previously or not
        InterruptCallbackFunction callbackFunc;                             // Function to be called when an interrupt is triggered

        void set_custom_time();                                             // Set a custom time on startup for the RTC to use
        void initializeRTC();                                               // Initialize RTC

        /* Sleep functionality */

        void pre_sleep();                                                   // Called just before the hypnos enters sleep, this disconnects the power rails and the serial bus
        void post_sleep(bool waitForSerial);                                // Called just after the hypnos wakes up, this reconnects the power rails and the serial bus

        

};