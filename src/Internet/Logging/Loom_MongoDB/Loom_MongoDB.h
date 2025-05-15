#pragma once

#include <ArduinoMqttClient.h>

#include "Loom_Manager.h"
#include "../MQTTComponent/MQTTComponent.h"
#include "../../Connectivity/NetworkComponent.h"

#include "../../../Hardware/Loom_BatchSD/Loom_BatchSD.h"



/**
 * Platform for logging data to MQTT for logging to a remote database
 * 
 * @author Will Richards
 */ 
class Loom_MongoDB : public MQTTComponent{
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
        Loom_MongoDB(
                Manager& man,
                NetworkComponent& internet_client, 
                const char* broker_address, 
                int broker_port, 
                const char* database_name, 
                const char* broker_user = "", 
                const char* broker_pass = "",
                const char* projectServer = ""
            );

        /**
         * Construct a new MQTT interface, expects credentials to be loaded from JSON
         * @param man Reference to the manager
         */ 
        Loom_MongoDB(Manager& man, NetworkComponent& internet_client);
        
        /**
         * Publish the current JSON data over MQTT 
         */ 
        bool publish() override;

        /**
         * Publish the current JSON data as a batch
         */ 
        bool publish(Loom_BatchSD& batchSD);

        /**
         * Publish metadata to the database
         */ 
        bool publishMetadata(char* metadata);

        /**
         * Load the MQTT credentials from a JSON string, used to pull credentials from a file
         * @param jsonString JSON formatted string containing the login credentials, this is freed at the end
         */
        void loadConfigFromJSON(char* json) override;
    
    private:
        Manager* manInst;                       // Instance of the manager   

        char topic[MAX_TOPIC_LENGTH];           // Topic we are publishing to
        char database_name[100];                // Database to publish the data to
        char projectServer[100];                // Project 

        

};