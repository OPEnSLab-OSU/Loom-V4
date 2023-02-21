#include "Loom_MQTT.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MQTT::Loom_MQTT(
                    Manager& man,
                    Client& internet_client, 
                    String broker_address, 
                    int broker_port, 
                    String database_name, 
                    String broker_user, 
                    String broker_pass
                ) : Module("MQTT"),
                    manInst(&man), 
                    internetClient(&internet_client), 
                    address(broker_address), 
                    port(broker_port), 
                    username(broker_user),
                    password(broker_pass) {
                        this->database_name = database_name;
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
    if(moduleInitialized){

        TIMER_DISABLE;
        if(mqttClient == nullptr){
            LOG("Creating new MQTT client!");
            mqttClient = new MqttClient(*internetClient);
        }

        // Formulate a topic to publish on with the format "DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chime1
        topic = database_name + "/" + (manInst->get_device_name() + String(manInst->get_instance_num()));

        // If we are logging in using credentials then supply them
        if(username.length() > 0)
            mqttClient->setUsernamePassword(username, password);

        // Set the keepalive timer
        mqttClient->setKeepAliveInterval(keep_alive);

        int retryAttempts = 0;

        // Try to connect multiple times as some may be dropped
        while(!mqttClient->connected() && retryAttempts < 5)
        {
            LOG("Attempting to connect to broker: " + address + ":" + String(port));

            // Attempt to Connect to the MQTT client 
            if(!mqttClient->connect(address.c_str(), port)){
                ERROR("Failed to connect to broker: " + getMQTTError());
                delay(5000);
            }

            // If our retry limit has been reached we dont want to try to send data cause it wont work
            if(retryAttempts == 4){
                ERROR("Retry limit exceeded!");
                TIMER_ENABLE;
                FUNCTION_END("void");
                return;
            }

            retryAttempts++;
        }
        
        LOG("Successfully connected to broker!");
        LOG("Attempting to send data...");

        // Tell the broker we are still here
        mqttClient->poll();

        // Start a message write the data and close the message
        if(mqttClient->beginMessage(topic, false, 2) != 1){
            ERROR("Failed to begin message!");
        }
        mqttClient->print(manInst->getJSONString());

        // Check to see if we are actually closing messages properly
        if(mqttClient->endMessage() != 1){
            ERROR("Failed to close message!");
        }
        else{
            LOG("Data has been successfully sent!");
        }   
    }
    else{
        WARNING("Module not initialized! If using credentials from SD make sure they are loaded first.");
    }
    FUNCTION_END("void");
    TIMER_ENABLE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MQTT::publish(Loom_BatchSD& batchSD){
    FUNCTION_START;
    if(moduleInitialized ){
        TIMER_DISABLE;

        if(mqttClient == nullptr){
            LOG("Creating new MQTT client!");
            mqttClient = new MqttClient(*internetClient);
        }

        if(batchSD.shouldPublish()){

            // Formulate a topic to publish on with the format "DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chime1
            topic = database_name + "/" + (manInst->get_device_name() + String(manInst->get_instance_num()));
            
            // If we are logging in using credentials then supply them
            if(username.length() > 0)
                mqttClient->setUsernamePassword(username, password);

            // Set the keepalive time
            mqttClient->setKeepAliveInterval(keep_alive);

            int retryAttempts = 0;

            // Try to connect multiple times as some may be dropped
            while(!mqttClient->connected() && retryAttempts < 5)
            {
                LOG("Attempting to connect to broker: " + address + ":" + String(port));

                // Attempt to Connect to the MQTT client 
                if(!mqttClient->connect(address.c_str(), port)){
                    ERROR("Failed to connect to broker: " + getMQTTError());
                    delay(5000);
                }

                // If our retry limit has been reached we dont want to try to send data cause it wont work
                if(retryAttempts == 4){
                    ERROR("Retry limit exceeded!");
                    TIMER_ENABLE;
                    FUNCTION_END("void");
                    return;
                }

                retryAttempts++;
            }
            
            LOG("Successfully connected to broker!");
            LOG("Attempting to send data...");

            // Tell the broker we are still here
            mqttClient->poll();
            
            std::vector<String> batch = batchSD.getBatch();
            for(int i = 0; i < batch.size(); i++){
                LOG("Publishing Packet " + String(i+1) + " of " + String(batch.size()));
                
                // Start a message write the data and close the message
                mqttClient->beginMessage(topic, false, 2);
                mqttClient->print(batch[i]);
                mqttClient->endMessage();
                delay(500);
            }
            
            LOG("Data has been successfully sent!");
            
        }
        else{
            LOG("Batch not ready to publish: " + String(batchSD.getCurrentBatch()) + "/" + String(batchSD.getBatchSize()));
        }
    }
    else{
        WARNING("Module not initialized! If using credentials from SD make sure they are loaded first.");
    }
    FUNCTION_END("void");
    TIMER_ENABLE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
String Loom_MQTT::getMQTTError(){
    FUNCTION_START;
    // Convert error codes to actual descriptions
    switch(mqttClient->connectError()){
        case -2:
            FUNCTION_END("CONNECTION_REFUSED");
            return String("CONNECTION_REFUSED");
        case -1:
            FUNCTION_END("CONNECTION_TIMEOUT");
            return String("CONNECTION_TIMEOUT");
        case 1:
            FUNCTION_END("UNACCEPTABLE_PROTOCOL_VERSION");
            return String("UNACCEPTABLE_PROTOCOL_VERSION");
        case 2:
            FUNCTION_END("IDENTIFIER_REJECTED");
            return String("IDENTIFIER_REJECTED");
        case 3:
            FUNCTION_END("SERVER_UNAVAILABLE");
            return String("SERVER_UNAVAILABLE");
        case 4:
            FUNCTION_END("BAD_USER_NAME_OR_PASSWORD");
            return String("BAD_USER_NAME_OR_PASSWORD");
        case 5:
            FUNCTION_END("NOT_AUTHORIZED");
            return String("NOT_AUTHORIZED");
        default:
            FUNCTION_END("UNKNOWN_ERROR");
            return String("UNKNOWN_ERROR");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MQTT::loadConfigFromJSON(String json){
    FUNCTION_START;
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    DeserializationError deserialError = deserializeJson(doc, json);

    // Check if an error occurred and if so print it
    if(deserialError != DeserializationError::Ok){
        ERROR("There was an error reading the MQTT credentials from SD: " + String(deserialError.c_str()));
    }
    
    address = doc["broker"].as<String>();
    port = doc["port"].as<int>();
    database_name = doc["database"].as<String>();
    username = doc["username"].as<String>();
    password = doc["password"].as<String>();

     // Formulate a topic to publish on with the format "DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chime1
    topic = database_name + "/" + (manInst->get_device_name() + String(manInst->get_instance_num()));
    moduleInitialized = true;
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////