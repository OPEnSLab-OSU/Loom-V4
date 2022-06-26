#pragma once

#include <ArduinoJson.h>
#include <vector>
#include <cstdarg>

#include "Module.h"

#define WAIT_TIME_MS 20000     // Time to wait for the serial interface to start
#define BAUD_RATE 115200        // Serial interface baud rate

/**
 * Unifies all the various sensors to allow for collection in unison
 * This class manages the JSON document store of all sensor information 
 */

class Manager{
    public:

        // Adds the module to the list of registered modules stored by the manager
        void registerModule(Module* module){modules.push_back(module);}; 

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
        };

        /**
         *  Calls the measure function to pull data from the sensors on all added modules
         */
        void measure() {
            for(int i = 0; i < modules.size(); i++){
                modules[i]->measure();
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
        void printJSON(){
            Serial.println("\n[Manager] Data Json: ");
            serializeJsonPretty(doc, Serial);
            Serial.println("\n");
        };
    
    private:
        StaticJsonDocument<2000> doc;        // JSON document that will store all sensor information
        std::vector<Module*> modules;        // List of modules that have been added to the stack

        uint32_t packetNumber = 1;           // Tracks the current packet number
};