#pragma once

#include "../Loom_Manager.h"
#include "Logger.h"
#include "../Hardware/Loom_Hypnos/Loom_Hypnos.h"
#include "../Sensors/Loom_Analog/Loom_Analog.h"
#include "../Radio/Loom_LoRa/Loom_LoRa.h"

#include <ArduinoJson.h>

class Loom_Heartbeat {
    public:

        Loom_Heartbeat(uint32_t normalWorkInterval, 
                       Manager* managerInstance, 
                       Loom_Hypnos* hypnosInstance,
                       Loom_LoRa* loraInstance = nullptr);

        // returns true if a heartbeat packet is meant to be transmitted
        // returns false if a normal packet is meant to be transmitted
        bool getHeartbeatFlag();

        // builds the heartbeat json doc and stores it in the heartbeat object
        void createDoc();

        // allows user to input strings and add to json document contents
        void addData(const char* module, const char* dataName, const char* data);

        // returns the heartbeat json document, for users to possibly edit
        // the contents and append information
        JsonDocument& getDoc();

        // does all construction of the heartbeat packet and information before
        // any transmissions occur
        void makeHeartbeat();

        // transmits the heartbeat document over lora
        bool transmit(const uint8_t destinationAddress);

        // function wrapper to transmit custom json packets
        bool transmitCustom(const uint8_t destinationAddress, JsonDocument& document);

    private:
        StaticJsonDocument<300> heartbeatDoc;

        Manager* managerInstance;
        Loom_Hypnos* hypnosInstance;
        Loom_LoRa* loraInstance;
        
        uint32_t normalWorkInterval = 0;
        uint32_t currentInterval = 0;
};