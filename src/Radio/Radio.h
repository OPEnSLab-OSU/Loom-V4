#pragma once

#include <SPI.h>
#include "../Module.h"
#include <ArduinoJson.h>

/**
 * Slightly abstracted Radio class to allow for all radio modules to inherit from one spot
 * 
 * @author Will Richards
 */ 
class Radio : public Module{
    protected:
        uint8_t deviceAddress;                  // Device address
        uint16_t maxMessageLength;              // Maximum length a packet can be
        int16_t signalStrength;                 // Strength of the signal received

        uint8_t powerLevel;                     // The power level we want to transmit at
        uint8_t retryCount;                     // Number transmission retries allowed
        uint16_t retryTimeout;                  // Delay between retries (MS)
        
        StaticJsonDocument<2000> messageJson;   // Where to store the received message

        /**
         * Get this device's address
         */ 
        uint8_t getAddress() const { return deviceAddress; };

        /**
         * Get the current signal strength of the radio
         */ 
        int16_t getSignalStrength() { return signalStrength; };

        /**
         * Receive a JSON packet from another radio, blocking until the wait time expires or a packet is received
         * @param maxWaitTime The maximum time to wait before continuing execution (Set to 0 for non-blocking)
         */ 
        virtual bool receive(uint maxWaitTime) = 0;

        /**
         * Send the current JSON data to the specified address
         * @param destinationAddress The address we want to send the data to
         */ 
        virtual bool send(const uint8_t destinationAddress) = 0;

        /**
         * Convert the message pack to json
         */ 
        bool bufferToJson(char* buffer, JsonDocument& json){

            // Clear the json to store new data
            messageJson.clear();
            String jsonStr = "";
            DeserializationError error = deserializeMsgPack(messageJson, buffer);

            // Check if an error occurred 
            if(error != DeserializationError::Ok){
                printModuleName(); Serial.println("Error occurred parsing MsgPack: " + String(error.c_str()));
                return false;
            }

            // Convert to string back into the main json document
            serializeJson(messageJson, jsonStr);
            deserializeJson(json, jsonStr);

            // Print out the received packet
            printModuleName(); Serial.println("\nMessage Received: ");
            serializeJsonPretty(messageJson, Serial);
            Serial.println("\n");

            return true;
        };

        /**
         * Convert the json to a message pack
         */ 
        bool jsonToBuffer(char* buffer, JsonObjectConst json){

            // Write all null-bytes
            memset(buffer, '\0', sizeof(buffer));

            bool status = serializeMsgPack(json, buffer, (size_t)maxMessageLength);

            return status;
        };

    public:

        /**
         * Construct a new Radio module
         * @param moduleName Name of the module
         * @param maxLength The maximum length a packet can be
         */ 
        Radio(String moduleName) : Module(moduleName) {};
};