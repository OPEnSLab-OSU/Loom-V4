#pragma once

#include <ArduinoJson.h>
#include <vector>

#include "Module.h"

#define WAIT_TIME_MS 20000     // Time to wait for the serial interface to start
#define BAUD_RATE 115200        // Serial interface baud rate

/**
 * Unifies all the various sensors to allow for collection in unison
 * This class manages the JSON document store of all sensor information 
 */

class Manager{
    public:

        /**
         * Registers a new sub-module to be controlled by the manager (Used on sensors so measure and package calls can all be called at once)
         * @param module Pointer to a class the inherits from Module that we want to add
         */ 
        void registerModule(Module* module){
            modules.push_back(module);
        }; 

        /**
         * Constructs a new Manager
         * @param devName Device name to provided for logging purposes
         * @param instanceNum Instance number for logging purposes
         */ 
        Manager(String devName, uint32_t instanceNum);

        /**
         * Get a reference to the JSON document that sensor data is stored in
         * @return reference to the main JSON document
         */ 
        StaticJsonDocument<2000>& getDocument(); // Returns a reference to the main JSON document storing 

        /**
         * Add a random piece of data to the overall JSON package in the given module name with a name for the data
         */ 
        template <typename T>
        void addData(String moduleName, String dataName, T data){
            doc[moduleName][dataName] = data;
        };

        /**
         *  Start the Serial interface with some parameters, should we wait up to 20 seconds for the serial interface to open before continuing 
         */ 
        void beginSerial(bool waitForSerial = true);

        /** 
         * Calls the initialization function on all added modules 
         */
        void initialize();

        /**
         *  Calls the measure function to pull data from the sensors on all added modules
         */
        void measure();

        /**
         *  Calls the package function to store all data from those sensors into a nice JSON package
         */
        void package();

        /**
         *  Calls the power_up function on each module to re-init after sleep
         */
        void power_up();

        /**
         *  Calls the power_down function on each module to safely enter sleep
         */
        void power_down();

        /**
         * Prints out the current JSON Document to the Serial bus
         */ 
        void display_data();

        /** 
         * Get a serialized version of the JSON packet as a string
         * @return JSON String
         */
        String getJSONString();
    
        /**
         * Gets the current device name set by the user
         * @return current device name
         */ 
        String get_device_name(){ return deviceName; };

        /**
         * Gets the current device instance number
         * @return current device instance number
         */ 
        int get_instance_num(){ return instanceNumber; };  
        
        /**
         * Get the unique serial number of the Feather m0
         * @return Unique serial number
         */ 
        String get_serial_num(){ return serial_num; }; 

        /**
         * Called by the Hypnos on construction to tell the manager it is in use
         */ 
        void useHypnos() { usingHypnos = true; };  

        /**
         * Set the current state of the hypnos enable
         */ 
        void setEnableState(bool state) { hypnosEnabled = state; };

    private:

        /* Device Information */
        String deviceName;                   // Name of the device
        uint32_t instanceNumber;             // Instance number of the device
        uint32_t packetNumber = 1;           // Tracks the current packet number
        String serial_num;

        void read_serial_num();              // Read the serial number out of the feather's registers
        

        /* Module Data */
        StaticJsonDocument<2000> doc;        // JSON document that will store all sensor information
        std::vector<Module*> modules;        // List of modules that have been added to the stack

        /* Validation */
        bool hasInitialized = false;         // Whether or not the initialize function has been called, if not it could be the source of hanging so we want to know

        bool usingHypnos = false;            // If the setup is using a hypnos
        bool hypnosEnabled = false;          // If the power rails on the hypnos are enabled this means we should be able to initalize

       
};