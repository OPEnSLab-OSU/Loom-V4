#pragma once



#include "Module.h"
#include "Loom_Manager.h"
#include "../../Logging/MQTTComponent/MQTTComponent.h"

#include "../../../Hardware/Loom_Hypnos/Loom_Hypnos.h"

#define MAX_TOPIC_LENGTH 512

/**
 * Remote management class handles, altering settings on-the-fly OTA
 * 
 * @author Will Richards
*/
class Loom_RemoteManager : public MQTTComponent{
    protected:
         /* These aren't used with the RemoteManager yet */
        void initialize() override { power_up(); }; 
        void measure() override {};                               
        void package() override {};

      

    public:
          /* Used with the manager */
        void power_down() override; 
        void power_up() override;
        
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
                const char* broker_pass = ""
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
        void loadConfigFromJSON(char* json) override;

        /**
         * Set an instance of the hypnos inside the RemoteManager
         * 
         * @param hypnos Reference to the Hypnos object
        */
        void setHypnosInstance(Loom_Hypnos& hypnos) { this->hypnosInst = &hypnos; };

        /* Publish the current status updates*/
        bool publish() override;
    
    private:
        Manager* manInst = nullptr;                                                         // Instance of the Loom Manager
        Loom_Hypnos* hypnosInst = nullptr;                                                  // Instance of the Hypno

        char topic[100];                                                                    // Where to publish the data to

        /* Helper methods for updating individual components of the device */

        /* General Device */
        void updateDeviceStatus(bool onOff);

        /* Hypnos */
        void updateHypnosInterval(char topic[MAX_TOPIC_LENGTH], char message[MAX_JSON_SIZE], StaticJsonDocument<MAX_JSON_SIZE> &json);
        void updateHypnosTime(char topic[MAX_TOPIC_LENGTH], char message[MAX_JSON_SIZE], StaticJsonDocument<MAX_JSON_SIZE> &json);
        
};