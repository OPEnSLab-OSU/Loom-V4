#pragma once


#include <ArduinoMqttClient.h>

#include "Module.h"
#include "Loom_Manager.h"

#include "../../../Hardware/Loom_Hypnos/Loom_Hypnos.h"

#define MAX_PACKET_SIZE 2000

/**
 * Remote management class handles, altering settings on-the-fly OTA
 * 
 * @author Will Richards
*/
class Loom_RemoteManager : public Module{
    protected:
         /* These aren't used with the RemoteManager yet */
        void initialize() override { power_up() }; 
        void measure() override {};                               
        void package() override {};

        /* Used with the manager */
        void power_down() override; 
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

        /**
         * Set an instance of the hypnos inside the RemoteManager
         * 
         * @param hypnos Reference to the Hypnos object
        */
        void setHypnosInstance(Loom_Hypnos& hypnos) { this->hypnosInst = &hypnos; };
    
    private:
        Manager* manInst = nullptr;                                                         // Instance of the Loom Manager
        Loom_Hypnos* hypnosInst = nullptr;                                                  // Instance of the Hypnos

        Client* internetClient;                                                             // Client to supply to the MQTT client to handle internet communication
        MqttClient mqttClient;                                                              // MQTT Client to manage interactions with the MQTT broker

        bool connectToBroker();                                                             // Connect to the MQTT broker
        void disconnectFromBroker();                                                        // Disconnect from the MQTT broker
        const char* getMQTTError();                                                         // Convert the MQTT error code into a string

        /* Publish a given message to a given topic with retain set to true */
        bool publishMessage(const char* topic, const char* message);                        // Publish a given message to a specific topic
        bool getCurrentRetained(const char* topic, char message[MAX_PACKET_SIZE]);          // Subscribes to a topic just to get the current retained message and then unsubscribes
        bool deleteRetained(const char* topic);                                             // Delete the last retained message

        /* Management Methods */
        bool refreshRemoteTopics();

        /* MQTT Parameters */
        int maxRetries = 4;                                                                 // How many times we want to retry the connection
        char address[100];                                                                  // Domain that the broker is running on
        int port;                                                                           // Port the broker is listening on
        char topic[100];                                                                    // Where to publish the data to
        char username[100];                                                                 // Username to log into the broker
        char password[100];                                                                 // Password to log into the broker

        /* Helper methods for updating individual components of the device */

        /* General Device */
        void updateDeviceStatus(bool onOff);

        /* Hypnos */
        void updateHypnosInterval(char topic[512], char message[MAX_PACKET_SIZE], StaticJsonDocument<MAX_PACKET_SIZE> &json);
        void updateHypnosTime(char topic[512], char message[MAX_PACKET_SIZE], StaticJsonDocument<MAX_PACKET_SIZE> &json);
        
};