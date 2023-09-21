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
        void power_down() override {}; 
        void package() override {};

        /* Used with the manager */
        void initialize() override;   
        void power_up() override;

    public:

        /**
         * Remote Manager constructor
         * 
         * @param man Reference to the Loom manager
         * @param internet_client Reference to a internet Client object
         * @param broker_address Address of the MQTT broker we are going to use
         * @param broker_port Broker connection port
         * @param broker_user Username to connect to the broker 
         * @param broker_pass Password to connect to the broker
        */
        Loom_RemoteManager(
                Manager& man, 
                Client& internet_client, 
                const char* broker_address, 
                int broker_port, 
                const char* broker_user = "", 
                const char* broker_pass = "",
            );
        
        /**
         * Barebones Constructor you will need to use loadConfigFromJSON to set the credentials
         * 
         * @param man Reference to the manager
         * @param internet_client Reference to the internet Client object
        */
        Loom_RemoteManager(Manager& man, Client& internet_client);

        /**
         * Load the MQTT credentials from a JSON string, used to pull credentials from a file
         * @param jsonString JSON formatted string containing the login credentials, this is freed at the end
         */
        void loadConfigFromJSON(char* json);

    private:
        Manager manInst = nullptr;                                      // Instance of the Loom Manager

        Client* internetClient;                                         // Client to supply to the MQTT client to handle internet communication
        MqttClient mqttClient;                                          // MQTT Client to manage interactions with the MQTT broker

        bool connectToBroker();                                         // Connect to the MQTT broker
        const char* getMQTTError();                                     // Convert the MQTT error code into a string

        /* Publish a given message to a given topic with retain set to true */
        bool publishMessage(const char* topic, const char* message);    // Publish a given message to a specific topic
        bool deleteRetained(const char* topic);                         // Delete the last retained message

        /* MQTT Parameters */
        int maxRetries = 4;                                             // How many times we want to retry the connection
        char address[100];                                              // Domain that the broker is running on
        int port;                                                       // Port the broker is listening on
        char topic[100];                                                // Where to publish the data to
        char username[100];                                             // Username to log into the broker
        char password[100];                                             // Password to log into the broker
};