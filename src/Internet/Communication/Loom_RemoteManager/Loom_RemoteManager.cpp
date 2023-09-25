#include "Loom_RemoteManager.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_RemoteManager::Loom_RemoteManager(
                Manager& man, 
                Client& internet_client, 
                const char* broker_address, 
                int broker_port, 
                const char* database_name, 
                const char* broker_user, 
                const char* broker_pass,
            ) : manInst(&man), Module("RemoteManager"), port(broker_port), internetClient(&internet_client), mqttClient(*internetClient){
                strncpy(this->address, broker_address, 100);
                strncpy(this->username, broker_user, 100);
                strncpy(this->password, password, 100);
            }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_RemoteManager::Loom_RemoteManager(Manager& man, Client& internet_client) : manInst(&man), Module("RemoteManager") {}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::power_up(){
    // Connect to the MQTT broker 
    if(connectToBroker()){
        refreshRemoteTopics();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::power_down(){
    // Disconnect from the broker
    updateDeviceStatus(false);
    disconnectFromBroker();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_RemoteManager::refreshRemoteTopics(){
    // Create topic name buffer and message buffer as well as a temp JSON document to parse the received packets into
    char topic[512];
    char message[MAX_PACKET_SIZE];
    StaticJsonDocument<MAX_PACKET_SIZE> tempDoc;

    // Update the current device status
    updateDeviceStatus(true);

    // Check if we are using a hypnos and then update the parameters about the hypnos
    if(hypnosInst != nullptr){

        // Update the hypnos sleep interval if there is anything to update
        updateHypnosInterval(topic, message, tempDoc);

        // Update the RTC time, if desired
        updateHypnosTime(topic, message, tempDoc);
        
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_RemoteManager::connectToBroker(){
    if(moduleInitialized){

        // If we are logging in using credentials then supply them
        if(strlen(username) > 0)
            mqttClient.setUsernamePassword(username, password);
        
        int retryAttempts = 0;

        // Try to connect multiple times as some may be dropped
        while(!mqttClient.connected())
        {
            // If our retry limit has been reached we dont want to try to send data cause it wont work
            if(retryAttempts >= maxRetries){
                ERROR(F("MQTT Retry limit exceeded!"));
                TIMER_ENABLE;
                FUNCTION_END;
                return false;
            }

            snprintf_P(output, OUTPUT_SIZE, PSTR("Attempting to connect to broker: %s:%i"), address, port);
            LOG(output);

            // Attempt to Connect to the MQTT client 
            if(!mqttClient.connect(address, port)){
                snprintf_P(output, OUTPUT_SIZE, PSTR("Failed to connect to broker: %s"), getMQTTError());
                ERROR(output);
                delay(5000);
            }

            retryAttempts++;
        }

        LOG(F("Successfully connected to broker!"));

        // Tell the broker we are still here
        mqttClient.poll();
        
        return true;
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::disconnectFromBroker(){
    mqttClient.stop();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_RemoteManager::publishMessage(const char* topic, const char* message){
    FUNCTION_START;

    // Make sure the module is initialized
    if(moduleInitialized){
        if(mqttClient.connected()){
            // Start a message write the data and close the message, publish all messages with retain
            if(mqttClient.beginMessage(topic, true, 2) != 1){
                ERROR(F("Failed to begin message!"));
            }

            // Print the message to the topic
            mqttClient.println(message);

            // Check to see if we are actually closing messages properly
            if(mqttClient.endMessage() != 1){
                ERROR(F("Failed to close message!"));
                FUNCTION_END;
                return false;
            }
            else{
                LOG(F("Data has been successfully sent!"));
                FUNCTION_END;
                return true;
            }   
        }
        else{
            ERROR("MQTT Client not connected to broker ")
        }
    }
    else{
        ERROR("Module not initialized!");
    }
    FUNCTION_END;
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_RemoteManager::getCurrentRetained(const char* topic, char message[MAX_PACKET_SIZE]){
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    if(mqttClient.connected()){
        /* Clear the incoming buffer */
        memset(message, '\0', MAX_PACKET_SIZE);

        // Subscribe to the given topic we want to read from
        if(!mqttClient.subscribe(topic)){
            snprintf(output, OUTPUT_SIZE, "Failed to subscribe to topic: %s.", topic);
            ERROR(output);
        }

        /* Attempt to parse a message on the current topic */
        int messageSize = mqttClient.parseMessage();
        if(messageSize){
            /* Copy the received message into the message string */
            strncpy(message, mqttClient.messageTopic().c_str(), MAX_PACKET_SIZE);

            // Unsubscribe from the topic 
            mqttClient.unsubscribe(topic);

            FUNCTION_END;
            return true;
        }
        else{
            WARNING("Message of size 0 received.");
            FUNCTION_END;
            return false;
            
        }
    }else{
        ERROR("Not connected to MQTT broker.");
        FUNCTION_END;
        return false;
    }
    

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_RemoteManager::deleteRetained(const char* topic) { publishMessage(topic, ""); }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::loadConfigFromJSON(char* json){
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    DeserializationError deserialError = deserializeJson(doc, json);

    /* 
        Initialize all to null bytes so they are treated as a 0 length string
        Issue: https://github.com/OPEnSLab-OSU/Loom-V4/issues/57
     */
    memset(username, '\0', 100);
    memset(password, '\0', 100);

    // Check if an error occurred and if so print it
    if(deserialError != DeserializationError::Ok){
        snprintf_P(output, OUTPUT_SIZE, PSTR("There was an error reading the MQTT credentials from SD: %s"), deserialError.c_str());
        ERROR(output);
    }
    

    // Only update values if not null
    if(!doc["broker"].isNull()){
        strncpy(address, doc["broker"].as<const char*>(), 100);

        // If we dont have a username don't try to update the variables
        if(!doc["username"].isNull()){
            strncpy(username, doc["username"].as<const char*>(), 100);
            strncpy(password, doc["password"].as<const char*>(), 100);
        }
       
        port = doc["port"].as<int>();
    }

    moduleInitialized = true;
    free(json);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::getMQTTError(){
    // Convert error codes to actual descriptions
    FUNCTION_START;
    switch(mqttClient.connectError()){
        case -2:
            FUNCTION_END;
            return "CONNECTION_REFUSED";
        case -1:
            FUNCTION_END;
            return "CONNECTION_TIMEOUT";
        case 1:
            FUNCTION_END;
            return "UNACCEPTABLE_PROTOCOL_VERSION";
        case 2:
            FUNCTION_END;
            return "IDENTIFIER_REJECTED";
        case 3:
            FUNCTION_END;
            return "SERVER_UNAVAILABLE";
        case 4:
            FUNCTION_END;
            return "BAD_USER_NAME_OR_PASSWORD";
        case 5:
            FUNCTION_END;
            return "NOT_AUTHORIZED";
        default:
            FUNCTION_END;
            return "UNKNOWN_ERROR";
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::updateHypnosInterval(char topic[512], char message[MAX_PACKET_SIZE], StaticJsonDocument<MAX_PACKET_SIZE> &json){
    // Clear message and topic and json
    memset(topic, '\0', 512);
    memset(message, '\0', MAX_PACKET_SIZE);
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
    snprintf(topic, 512, "RemoteManager/%s%i/Hypnos/setSleepInterval", manInst->get_device_name(), manInst->get_instance_num());
    if(getCurrentRetained((const char*)topic, message)){

        // Parse the incoming message into a JSON Document and then create a new date time from the values to update the current time in the Hypnos
        deserializeJson(json, (const char*)message);
        DateTime time = DateTime(json["days"].as<int>(), json["hours"].as<int>(), json["minutes"].as<int>(), json["seconds"].as<int>());
        
        // Set the new interrupt duration
        hypnosInst->setInterruptDuration(time);
        
        // And then delete the current retained message so we don't update it again
        deleteRetained((const char*) topic);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::updateHypnosTime(char topic[512], char message[MAX_PACKET_SIZE], StaticJsonDocument<MAX_PACKET_SIZE> &json){
    // Clear message and topic and json
    memset(topic, '\0', 512);
    memset(message, '\0', MAX_PACKET_SIZE);
    json.clear();

    /* 
        This is the topic that we need to publish to to change the RTC time. The contents of the packet can be whatever there just needs to be a packet
    */
    snprintf(topic, 512, "RemoteManager/%s%i/Hypnos/setRTC", manInst->get_device_name(), manInst->get_instance_num());
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
    char topic[512];

    // Clear strings
    memset(message, '\0', 100);
    memset(topic, '\0', 512);

    // Set the online flag and then serialize the json to a string
    json["online"] = onOff;
    serializeJson(json, message);

    // Format the topic to publish the data to and publish the message
    snprintf(topic, 512, "RemoteManager/%s%i/status", manInst->get_device_name(), manInst->get_instance_num());
    publishMessage((const char*)topic, message);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////



