#pragma once

#include <SPI.h>
#include "Logger.h"
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

        StaticJsonDocument<255> recvDoc;        // Individual Document Representing what is being received
        StaticJsonDocument<255> sendDoc;        // Individual Document Representing what is being sent
        
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
        bool bufferToJson(uint8_t* buffer){
            char output[100];
            DeserializationError error = deserializeMsgPack(recvDoc, buffer, maxMessageLength);

            // Check if an error occurred 
            if(error != DeserializationError::Ok){
                snprintf(output, 100, "Error occurred parsing MsgPack: %s", error.c_str());
                ERROR(output);
                return false;
            }

            return true;
        };

        /**
         * Convert the json to a message pack
         */ 
        bool jsonToBuffer(uint8_t* buffer, JsonObjectConst json){
            bool status = serializeMsgPack(json, buffer, maxMessageLength);

            return status;
        };

    public:

        /**
         * Construct a new Radio module
         * @param moduleName Name of the module
         * @param maxLength The maximum length a packet can be
         */ 
        Radio(const char* moduleName) : Module(moduleName) {};
};