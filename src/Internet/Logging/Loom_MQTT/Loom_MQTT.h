#pragma once

#include <ArduinoMqttClient.h>

#include "Loom_Manager.h"
#include "Module.h"

#include "../../../Hardware/Loom_BatchSD/Loom_BatchSD.h"



/**
 * Platform for logging data to MQTT for logging to a remote database
 * 
 * @author Will Richards
 */ 
class Loom_MQTT : public Module{
    protected:

        /* These aren't used with the MQTT */
        void measure() override {};                               
        
        void initialize() override {};    
        void power_up() override {};
        void power_down() override {}; 
        void package() override {};

    public: 

        /**
         * Construct a new MQTT interface
         * @param man Reference to the manager
         * @param internet_client Reference to whatever connectivity platform is being used
         * @param broker_address Domain where the broker is being hosted
         * @param broker_port Port that the broker is listening on
         * @param database_name Name of the database that will be used by MongoDB
         * 
         * Not Required:
         * @param broker_user User name to log into the broker
         * @param broker_pass Password to log into the broker
         */ 
        Loom_MQTT(
                Manager& man,
                Client& internet_client, 
                const char* broker_address, 
                int broker_port, 
                const char* database_name, 
                const char* broker_user = "", 
                const char* broker_pass = "",
            );

        /**
         * Construct a new MQTT interface, expects credentials to be loaded from JSON
         * @param man Reference to the manager
         */ 
        Loom_MQTT(Manager& man, Client& internet_client);
        
        /**
         * Publish the current JSON data over MQTT 
         */ 
        void publish();

        /**
         * Publish the current JSON data as a batch
         */ 
        void publish(Loom_BatchSD& batchSD);

        /**
         * Length of time the broker should keep the connection open for default 
         * @param time Length of time in MILLISECONDS the connection will be kept open
         */ 
        void setKeepAlive(int time) { keep_alive = time; };
        
        /**
         * Set the maximum number of reconnection attempts to make before failing
         * @param retries The number of retries we want to make
        */
        void setMaxRetries(int retries) { maxRetries = retries; };

        /**
         * Load the MQTT credentials from a JSON string, used to pull credentials from a file
         * @param jsonString JSON formatted string containing the login credentials, this is freed at the end
         */
        void loadConfigFromJSON(char* json);
    
    private:
        Manager* manInst;                       // Instance of the manager

        Client* internetClient;                  // Client to supply to the MQTT client to handle internet communication
        MqttClient mqttClient;                  // MQTT Client to manage interactions with the MQTT broker

        const char* getMQTTError();                  // Get the string representation of the MQTT error codes
    
        int keep_alive = 60000;                 // How long the broker should keep the connection open, defaults to a minute
        int maxRetries = 4;                         // How many times we want to retry the connection

        char address[100];                         // Domain that the broker is running on
        char database_name[100];                   // Database to publish the data to
        int port;                                   // Port the broker is listening on
        char topic[100];                           // Where to publish the data to
        char username[100];                        // Username to log into the broker
        char password[100];                        // Password to log into the broker

        

};