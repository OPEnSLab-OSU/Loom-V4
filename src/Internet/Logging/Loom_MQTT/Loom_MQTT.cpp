#include "Loom_MQTT.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MQTT::Loom_MQTT(
                    Manager& man,
                    Client& internet_client, 
                    const char* broker_address, 
                    int broker_port, 
                    const char* database_name, 
                    const char* broker_user, 
                    const char* broker_pass
                ) : Module("MQTT"),
                    manInst(&man), 
                    internetClient(&internet_client), 
                    port(broker_port) 
                    {
                        strncpy(this->address, broker_address, 100);
                        strncpy(this->database_name, database_name, 100);
                        strncpy(this->username, broker_user, 100);
                        strncpy(this->password, broker_pass, 100);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MQTT::Loom_MQTT(Manager& man, Client& internet_client) : Module("MQTT"), manInst(&man), internetClient(&internet_client){
    moduleInitialized = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MQTT::~Loom_MQTT(){
    delete mqttClient;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MQTT::publish(){
    FUNCTION_START;
    char output[100];
    if(moduleInitialized){

        TIMER_DISABLE;
        if(mqttClient == nullptr){
            LOG(F("Creating new MQTT client!"));
            mqttClient = new MqttClient(*internetClient);
        }
        
        // Formulate a topic to publish on with the format "DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chime1
        snprintf(topic, 100, "%s/%s%i", database_name, manInst->get_device_name(), manInst->get_instance_num());

        // If we are logging in using credentials then supply them
        if(strlen(username) > 0)
            mqttClient->setUsernamePassword(username, password);

        // Set the keepalive timer
        mqttClient->setKeepAliveInterval(keep_alive);

        int retryAttempts = 0;

        // Try to connect multiple times as some may be dropped
        while(!mqttClient->connected() && retryAttempts < 5)
        {
            snprintf(output, 100, "Attempting to connect to broker: %s:%i", address, port);
            LOG(output);

            // Attempt to Connect to the MQTT client 
            if(!mqttClient->connect(address, port)){
                snprintf(output, 100, "Attempting to connect to broker: %s:%i", address, port);
                ERROR(output);
                delay(5000);
            }

            // If our retry limit has been reached we dont want to try to send data cause it wont work
            if(retryAttempts == 4){
                ERROR(F("Retry limit exceeded!"));
                TIMER_ENABLE;
                FUNCTION_END;
                return;
            }

            retryAttempts++;
        }
        
        LOG(F("Successfully connected to broker!"));
        LOG(F("Attempting to send data..."));

        // Tell the broker we are still here
        mqttClient->poll();

        // Start a message write the data and close the message
        if(mqttClient->beginMessage(topic, false, 2) != 1){
            ERROR(F("Failed to begin message!"));
        }
        mqttClient->print(manInst->getJSONString());

        // Check to see if we are actually closing messages properly
        if(mqttClient->endMessage() != 1){
            ERROR(F("Failed to close message!"));
        }
        else{
            LOG(F("Data has been successfully sent!"));
        }   
    }
    else{
        WARNING(F("Module not initialized! If using credentials from SD make sure they are loaded first."));
    }
    FUNCTION_END;
    TIMER_ENABLE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MQTT::publish(Loom_BatchSD& batchSD){
    FUNCTION_START;
    char output[100];
    if(moduleInitialized ){
        TIMER_DISABLE;

        if(mqttClient == nullptr){
            LOG(F("Creating new MQTT client!"));
            mqttClient = new MqttClient(*internetClient);
        }

        if(batchSD.shouldPublish()){

            // Formulate a topic to publish on with the format "DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chime1
            snprintf(topic, 100, "%s/%s%i", database_name, manInst->get_device_name(), manInst->get_instance_num());
            
            // If we are logging in using credentials then supply them
            if(strlen(username) > 0)
                mqttClient->setUsernamePassword(username, password);

            // Set the keepalive time
            mqttClient->setKeepAliveInterval(keep_alive);

            int retryAttempts = 0;

            // Try to connect multiple times as some may be dropped
            while(!mqttClient->connected() && retryAttempts < 5)
            {
                snprintf(output, 100, "Attempting to connect to broker: %s:%i", address, port);
                LOG(output);

                // Attempt to Connect to the MQTT client 
                if(!mqttClient->connect(address, port)){
                    snprintf(output, 100, "Failed to connect to broker: %s", getMQTTError());
                    ERROR(output);
                    delay(5000);
                }

                // If our retry limit has been reached we dont want to try to send data cause it wont work
                if(retryAttempts == 4){
                    ERROR(F("Retry limit exceeded!"));
                    TIMER_ENABLE;
                    FUNCTION_END;
                    return;
                }

                retryAttempts++;
            }
            
            LOG(F("Successfully connected to broker!"));
            LOG(F("Attempting to send data..."));

            // Tell the broker we are still here
            mqttClient->poll();

            // Pass batch vector in as a reference 
            std::vector<String> batch;
            batchSD.getBatch(batch);

            for(int i = 0; i < batch.size(); i++){
                snprintf(output, 100, "Publishing Packet %i of %d", i+1, batch.size());
                LOG(output);
                
                // Start a message write the data and close the message
                mqttClient->beginMessage(topic, false, 2);
                mqttClient->print(batch[i]);
                mqttClient->endMessage();
                delay(500);
            }
            
            LOG(F("Data has been successfully sent!"));
            
        }
        else{
            snprintf(output, 100, "Batch not ready to publish: %i/%i", batchSD.getCurrentBatch(), batchSD.getBatchSize());
            LOG(output);
        }
    }
    else{
        WARNING(F("Module not initialized! If using credentials from SD make sure they are loaded first."));
    }
    FUNCTION_END;
    TIMER_ENABLE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
const char* Loom_MQTT::getMQTTError(){
    FUNCTION_START;
    // Convert error codes to actual descriptions
    switch(mqttClient->connectError()){
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
void Loom_MQTT::loadConfigFromJSON(char* json){
    FUNCTION_START;
    char output[100];
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    DeserializationError deserialError = deserializeJson(doc, json);

    // Check if an error occurred and if so print it
    if(deserialError != DeserializationError::Ok){
        snprintf(output, 100, "There was an error reading the MQTT credentials from SD: %s", deserialError.c_str());
        ERROR(output);
    }

    // Only update values if not null
    if(!doc["broker"].isNull()){
        strncpy(address, doc["broker"].as<const char*>(), 100);
        strncpy(database_name, doc["database"].as<const char*>(), 100);
        strncpy(username, doc["username"].as<const char*>(), 100);
        strncpy(password, doc["password"].as<const char*>(), 100);
        port = doc["port"].as<int>();
    }
    
    // Formulate a topic to publish on with the format "DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chime1
    snprintf(topic, 100, "%s/%s%i", database_name, manInst->get_device_name(), manInst->get_instance_num());
    moduleInitialized = true;
    free(json);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////