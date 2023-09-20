#pragma once


#include <ArduinoMqttClient.h>

#include "Module.h"
#include "Loom_Manager.h"

/**
 * Remote management class handles, altering settings on-the-fly OTA
 * 
 * @author Will Richards
*/
class Loom_RemoteManager : public Module{
    protected:
         /* These aren't used with the RemoteManager yet */
        void measure() override {};                               
        void initialize() override {};    
        void power_up() override {};
        void power_down() override {}; 
        void package() override {};

    public:

        /**
         * Remote Manager constructor
         * 
         * @param man Reference to the Loom manager
        */
        Loom_RemoteManager(Manager& man, );
        

        /**
         * Load the MQTT credentials from a JSON string, used to pull credentials from a file
         * @param jsonString JSON formatted string containing the login credentials, this is freed at the end
         */
        void loadConfigFromJSON(char* json);

    private:
        Manager manInst = nullptr;          // Instance of the Loom Manager

        Client* internetClient;                  // Client to supply to the MQTT client to handle internet communication
        MqttClient mqttClient;                  // MQTT Client to manage interactions with the MQTT broker
};