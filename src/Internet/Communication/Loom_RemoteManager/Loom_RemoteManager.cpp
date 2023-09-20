#include "Loom_RemoteManager.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_RemoteManager::Loom_RemoteManager(Manager& man, ) : manInst(&man), Module("RemoteManager") {}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::loadConfigFromJSON(char* json){
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

    // Only update values if not null
    if(!doc["broker"].isNull()){
        strncpy(address, doc["broker"].as<const char*>(), 100);
        strncpy(database_name, doc["database"].as<const char*>(), 100);
        strncpy(username, doc["username"].as<const char*>(), 100);
        strncpy(password, doc["password"].as<const char*>(), 100);
        strncpy(projectServer, doc["project"].as<const char*>(), 100);
        port = doc["port"].as<int>();
    }
    
    if(strlen(projectServer) > 0)
            // Formulate a topic to publish on with the format "ProjectName/DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chimes/Chime1
            snprintf_P(topic, 100, PSTR("%s/%s/%s%i"), projectServer, database_name, manInst->get_device_name(), manInst->get_instance_num());
        else
            // Formulate a topic to publish on with the format "DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chime1
            snprintf_P(topic, 100, PSTR("%s/%s%i"), database_name, manInst->get_device_name(), manInst->get_instance_num());
    
    moduleInitialized = true;
    free(json);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////