#pragma once

#include <OPEnS_RTC.h>
#include <LowPower.h>

#include "Arduino.h"
#include "Module.h"
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
        void package() override {};                                
        void print_measurements() override {};
        void initialize() override {};    
        void power_up() override {};
        void power_down() override {};                         
    public:

        /**
         * Constructs a new Hypnos Instance
         * @param version The version of the Hypnos in use, this changes which pin is used as and SD chip select
         * @param useSD Whether or not SD card functionality should be enabled
         */ 
        Loom_Hypnos(HYPNOS_VERSION version, bool useSD = true);
        Loom_Hypnos(Manager& man, HYPNOS_VERSION version, bool useSD = true);

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
    
    private:

        Manager* manInst;

        int sd_chip_select;                                                 // Pin that the SD card will use to communicate with the Hypnos
        bool enableSD;                                                      // Specifies whether or not the SD card should be enabled on the Hypnos

        /* Real-Time Clock Settings */

        RTC_DS3231 RTC_DS;                                                  // Real time clock reference
        bool RTC_initialized = false;                                       // Did the RTC initialize correctly?

        bool hasInterruptBeenRegistered = false;                            // If we have actually registered and interrupt previously or not
        InterruptCallbackFunction callbackFunc;                             // Function to be called when an interrupt is triggered

        void initializeRTC();                                               // Initialize the real-time clock present on the Hypnos

        /* Sleep functionality */

        void pre_sleep();                                                   // Called just before the hypnos enters sleep, this disconnects the power rails and the serial bus
        void post_sleep(bool waitForSerial);                                // Called just after the hypnos wakes up, this reconnects the power rails and the serial bus

        

};