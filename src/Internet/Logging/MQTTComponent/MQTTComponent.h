#pragma once

#include <ArduinoMqttClient.h>

#include "Module.h"

#define MAX_PACKET_SIZE 2000                // The maximum length of an MQTT message
#define MAX_TOPIC_LENGTH 512                // The maximum length of a topic string

/**
 * MQTT Abstraction class that provides basic MQTT communciation functionality
 * 
 * @author Will Richards
*/
class MQTTComponent : public Module{
    protected:

        /** 
         * Set the backbones MQTT parameters 
         * 
         * @param address The address of the MQTT broker
         * @param port The port that the MQTT broker is listening on
        */
        void setConnectionParameters(const char* address, int port, const char* username, const char* password);

        bool connectToBroker();                                                                         // Connect to the configured broker
        void disconnectFromBroker() { mqttClient.stop(); };                                             // Disconnect from the MQTT broker
        const char* getMQTTError();                                                                     // Convert the MQTT error code into a string

        /**
         * Publish an MQTT message to a given topic and broker
         * 
         * @param topic The MQTT topic we want to publish our message to
         * @param message The message we want to publish to the given topic
         * @param retain Whether or not we want to the message to be retained on the specified topic (default = false)
         * @param qos What quality-of-service we want to upload the message with (default = 2)
         * 
         * @return The status of the publish attempt
        */
        bool publishMessage(const char* topic, const char* message, bool retain = false, int qos = 2); 

        /**
         * Subscribe to a given topic to get the retained message and then immediately unsubscribe
         * 
         * @param topic The topic we want to retrieve the message from
         * @param message The buffer we want to store the retrieved message in
         * 
         * @param The status of the retrieval attempt
        */
        bool getCurrentRetained(const char* topic, char message[MAX_PACKET_SIZE]);         

        /**
         * Publishes a message to a given topic with size 0 and the retain flag set to true so that the current retained message is removed
         * 
         * @param topic The topic we want to delete the retained message on
         * 
         * @return The result of the deletion attempt
         * */             
        bool deleteRetained(const char* topic);

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

        /* On power up we want to connect to the broker, this can be overridden but you will need to call connectToBroker again */
        virtual void power_up() override { connectToBroker(); };    

        /* On initialize we just want to call the power_up function to connect to the broker, this can also be overriden but you should call power_up or connectToBroker again */
        virtual void initialize() override { 
            if(strlen(address) <= 0 || port = 0){
                moduleInitialized = false;
                ERROR("Broker address not specified, module will be uninitialized.");
            }else{
                power_up(); 
            }
        };           

    public:

        /**
         * Constructor for base MQTTComponent
         * 
         * @param compName Name for the underlying module
         * @param internet_client The Client object from a internet platform 
        */
        MQTTComponent(const char* compName, Client& internet_client) : Module(compName), mqttClient(*internet_client) {
            /* Clear all connection parameters */
            memset(address, '\0', 100);
            port = 0;
            memset(username, '\0', 100);
            memset(password, '\0', 100);
        };

        /* Publishes the current sample to the remote broker */
        void publish() = 0;

        /**
         * Load the MQTT credentials from a JSON string, used to pull credentials from a file
         * @param jsonString JSON formatted string containing the login credentials, this is freed at the end
         */
        void loadConfigFromJSON(char* json) = 0;

    private:
        MqttClient mqttClient;                      // Instance of the MQTT client

        int keep_alive = 60000;                     // How long the broker should keep the connection open, defaults to a minute
        int maxRetries = 4;                         // How many times we want to retry the connection

        /* MQTT Connection parameters */
        char address[100];                          // Domain that the broker is running on
        int port;                                   // Port the broker is listening on
        char username[100];                         // Username to log into the broker
        char password[100];                         // Password to log into the broker

};        