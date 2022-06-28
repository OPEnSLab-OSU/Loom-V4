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
        Manager(String devName, uint32_t instanceNum) : deviceName(devName), instanceNumber(instanceNum) {};

        /**
         * Get a reference to the JSON document that sensor data is stored in
         * @return reference to the main JSON document
         */ 
        StaticJsonDocument<2000>& getDocument() {return doc;}; // Returns a reference to the main JSON document storing 

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
        void beginSerial(bool waitForSerial = true){
            long startMillis = millis();

            Serial.begin(BAUD_RATE);
            // Pause if the Serial is not open and we want to wait
            while(!Serial && waitForSerial){

                // If it has been 20 seconds break out of the loop
                if(millis() >= (startMillis+WAIT_TIME_MS)){
                    break;
                }
            }
        }

        /** 
         * Calls the initialization function on all added modules 
         */
        void initialize() {
            Serial.println("[Manager] Initializing Modules...");
            for(int i = 0; i < modules.size(); i++){
                modules[i]->initialize();
            }
            hasInitialized = true;
        };

        /**
         *  Calls the measure function to pull data from the sensors on all added modules
         */
        void measure() {
            if(hasInitialized){
                for(int i = 0; i < modules.size(); i++){
                    modules[i]->measure();
                }
            }
            else{
                 Serial.println("[Manager] Unable to collect data as the manager and thus all sensors connected to it have not been initialized! Call manager.initialize() to fix this.");
            }
        };

        /**
         *  Calls the package function to store all data from those sensors into a nice JSON package
         */
        void package(){
            // Display the packet number
            doc["Packet"]["Number"] = packetNumber;

            for(int i = 0; i < modules.size(); i++){
                modules[i]->package();
            }
            packetNumber++;
        };

        /**
         *  Calls the power_up function on each module to re-init after sleep
         */
        void power_up(){
            for(int i = 0; i < modules.size(); i++){
                modules[i]->power_up();
            }
        };

        /**
         *  Calls the power_down function on each module to safely enter sleep
         */
        void power_down(){
            for(int i = 0; i < modules.size(); i++){
                modules[i]->power_down();
            }
        };

        /**
         * Prints out the current JSON Document to the Serial bus
         */ 
        void display_data(){
            Serial.println("\n[Manager] Data Json: ");
            serializeJsonPretty(doc, Serial);
            Serial.println("\n");
        };
    
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

    private:

        /* Device Information */
        String deviceName;                   // Name of the device
        uint32_t instanceNumber;             // Instance number of the device
        uint32_t packetNumber = 1;           // Tracks the current packet number

        /* Module Data */
        StaticJsonDocument<2000> doc;        // JSON document that will store all sensor information
        std::vector<Module*> modules;        // List of modules that have been added to the stack

        /* Validation */
        bool hasInitialized = false;         // Whether or not the initialize function has been called, if not it could be the source of hanging so we want to know

       
};