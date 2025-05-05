#include "Loom_RemoteManager.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_RemoteManager::Loom_RemoteManager(
                Manager& man, 
                NetworkComponent& internet_client, 
                const char* broker_address, 
                int broker_port, 
                const char* broker_user, 
                const char* broker_pass
            ) : manInst(&man), MQTTComponent("RemoteManager", internet_client){
                strncpy(this->address, broker_address, 100);
                port = broker_port;
                strncpy(this->username, broker_user, 100);
                strncpy(this->password, broker_pass, 100);
                manInst->registerModule(this);
            }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_RemoteManager::Loom_RemoteManager(Manager& man, NetworkComponent& internet_client) : manInst(&man), MQTTComponent("RemoteManager", internet_client) {manInst->registerModule(this);}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::power_up(){
    // Connect to the MQTT broker 
    if(connectToBroker()){
        publish();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::power_down(){
    // Disconnect from the broker
    disconnectFromBroker();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_RemoteManager::publish(){
    // Create topic name buffer and message buffer as well as a temp JSON document to parse the received packets into
    char topic[MAX_TOPIC_LENGTH];
    char message[MAX_JSON_SIZE];
    StaticJsonDocument<MAX_JSON_SIZE> tempDoc;
    
    // Update the current device status
    updateDeviceStatus(true);

    // Check if we are using a hypnos and then update the parameters about the hypnos
    if(hypnosInst != nullptr){

        // Update the hypnos sleep interval if there is anything to update
        updateHypnosInterval(topic, message, tempDoc);

        // Update the RTC time, if desired
        updateHypnosTime(topic, message, tempDoc);
        
    }

     updateDeviceStatus(false);

    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::loadConfigFromJSON(char* json){
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    DeserializationError deserialError = deserializeJson(doc, (const char *)json);

    // Check if an error occurred and if so print it
    if(deserialError != DeserializationError::Ok){
        snprintf_P(output, OUTPUT_SIZE, PSTR("There was an error reading the MQTT credentials from SD: %s"), deserialError.c_str());
        ERROR(output);
    }

    // Clear the strings and set port = 0
    memset(address, '\0', 100);
    memset(username, '\0', 100);
    memset(password, '\0', 100);
    port = 0;
    
    /* We should check if any parameter is null */
    if(!doc["broker"].isNull())
        strncpy(address, doc["broker"].as<const char*>(), 100);

    if(!doc["username"].isNull())
        strncpy(username, doc["username"].as<const char*>(), 100);

    if(!doc["password"].isNull())
        strncpy(password, doc["password"].as<const char*>(), 100);

    if(!doc["port"].isNull())
        port = doc["port"].as<int>();
    
    free(json);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::updateHypnosInterval(char topic[MAX_TOPIC_LENGTH], char message[MAX_JSON_SIZE], StaticJsonDocument<MAX_JSON_SIZE> &json){
    // Clear message and topic and json
    memset(topic, '\0', MAX_TOPIC_LENGTH);
    memset(message, '\0', MAX_JSON_SIZE);
    json.clear();

    /* 
        This is the topic that we need to publish to to change the sleep interval.
        The packet published here by the remote management interface should be as follows
        {
            "days": 0, 
            "hours": 0, 
            "minutes": 0,
            "seconds": 0
        }
    */
    snprintf(topic, MAX_TOPIC_LENGTH, "RemoteManager/%s%i/Hypnos/setSleepInterval", manInst->get_device_name(), manInst->get_instance_num());
    if(getCurrentRetained((const char*)topic, message)){

        // Parse the incoming message into a JSON Document and then create a new date time from the values to update the current time in the Hypnos
        deserializeJson(json, (const char*)message);
        TimeSpan time = TimeSpan(json["days"].as<int>(), json["hours"].as<int>(), json["minutes"].as<int>(), json["seconds"].as<int>());
        
        // Set the new interrupt duration
        hypnosInst->setInterruptDuration(time);
        
        // And then delete the current retained message so we don't update it again
        deleteRetained((const char*) topic);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::updateHypnosTime(char topic[MAX_TOPIC_LENGTH], char message[MAX_JSON_SIZE], StaticJsonDocument<MAX_JSON_SIZE> &json){
    // Clear message and topic and json
    memset(topic, '\0', MAX_TOPIC_LENGTH);
    memset(message, '\0', MAX_JSON_SIZE);
    json.clear();

    /* 
        This is the topic that we need to publish to to change the RTC time. The contents of the packet can be whatever there just needs to be a packet
    */
    snprintf(topic, MAX_TOPIC_LENGTH, "RemoteManager/%s%i/Hypnos/setRTC", manInst->get_device_name(), manInst->get_instance_num());
    if(getCurrentRetained((const char*)topic, message)){
        // Set the new RTC time from the network
        hypnosInst->networkTimeUpdate();
        
        // And then delete the current retained message so we don't update it again
        deleteRetained((const char*) topic);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::updateDeviceStatus(bool onOff){
    StaticJsonDocument<100> json;
    char message[100];
    char topic[MAX_TOPIC_LENGTH];

    // Clear strings
    memset(message, '\0', 100);
    memset(topic, '\0', MAX_TOPIC_LENGTH);

    // Set the online flag and then serialize the json to a string
    json["online"] = onOff;
    serializeJson(json, message);

    // Format the topic to publish the data to and publish the message
    snprintf(topic, MAX_TOPIC_LENGTH, "RemoteManager/%s%i/status", manInst->get_device_name(), manInst->get_instance_num());
    publishMessage((const char*)topic, message);
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////



