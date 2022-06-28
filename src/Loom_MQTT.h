#pragma once

#include <ArduinoMqttClient.h>

#include "Loom_Manager.h"
#include "Module.h"

/**
 * Platform for logging data to MQTT for logging to a remote database
 */ 
class Loom_MQTT : public Module{
    protected:

        /* These aren't used with the MQTT */
        void measure() override {};                               
        void print_measurements() override {};
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
         * @param broker_pass Password to log into the borker
         */ 
        Loom_MQTT(
                Manager& man,
                Client& internet_client, 
                String broker_address, 
                int broker_port, 
                String database_name, 
                String broker_user = "", 
                String broker_pass = ""
            );
        
        /**
         * Publish the current JSON data over MQTT 
         */ 
        void publish();

        /**
         * Length of time the broker should keep the connection open for default 
         * @param time Length of time in MINUTES the connection will be kept open
         */ 
        void setKeepAlive(int time) { keep_alive = time; };
    
    private:
        Manager* manInst;           // Instance of the manager
        MqttClient mqttClient;      // MQTT Client to manage interactions with the MQTT broker

        String getMQTTError();      // Get the string representation of the MQTT error codes

        int keep_alive = 60000;     // How long the broker should keep the connection open, defaults to a minute

        String address;             // Domain that the broker is running on
        int port;                   // Port the broker is listening on
        String topic;
        String username;            // Username to log into the broker
        String password;            // Password to log into the broker

        

};