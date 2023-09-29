#include "Loom_ThingSpeak.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ThingSpeak::Loom_ThingSpeak(
                    Manager& man,
                    Client& internet_client, 
                    const char* broker_address, 
                    int broker_port, 
                    int channelID, 
                    const char* broker_user, 
                    const char* broker_pass,
                ) : MQTTComponent("ThingSpeak", internet_client),
                    manInst(&man)
                    {
                        /* MQTT Connection parameters */
                        strncpy(this->address, broker_address, 100);
                        port = broker_port;
                        strncpy(this->username, broker_user, 100);
                        strncpy(this->password, broker_pass, 100);
                        this->channelID = channelID;
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ThingSpeak::Loom_ThingSpeak(Manager& man, Client& internet_client) : MQTTComponent("ThingSpeak", internet_client), manInst(&man){
    moduleInitialized = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_ThingSpeak::publish(){
    FUNCTION_START;
    char message[2000];
    char topic[MAX_TOPIC_LENGTH];
    if(moduleInitialized){
        TIMER_DISABLE;

        /* Attempt to connect to the broker if it fails we should just return */
        if(!connectToBroker()){
            FUNCTION_END;
            return false;
        }

        /* Publish the message to the given topic */
        if(!publishMessage(topic, jsonString, false, 0)){
            FUNCTION_END;
            return false;
        }
    }
    else{
        WARNING(F("Module not initialized! If using credentials from SD make sure they are loaded first."));
        FUNCTION_END;
        return false;
    }
    FUNCTION_END;
    TIMER_ENABLE;
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ThingSpeak::loadConfigFromJSON(char* json){
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    DeserializationError deserialError = deserializeJson(doc, json);

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

    if(!doc["channelID"].isNull())
        channelID = doc["channelID"].as<int>();
    
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