#include "Loom_MQTT.h"

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
                    mqttClient(internet_client), 
                    address(broker_address), 
                    port(broker_port), 
                    username(broker_user),
                    password(broker_pass) {
                        this->database_name = database_name;

                        // Formulate a topic to publish on with the format "DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chime1
                        topic = database_name + "/" + (manInst->get_device_name() + String(manInst->get_instance_num()));
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MQTT::Loom_MQTT(Manager& man, Client& internet_client) : Module("MQTT"), manInst(&man), mqttClient(internet_client){
    moduleInitialized = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MQTT::publish(){
    if(moduleInitialized){
        // If we are logging in using credentials then supply them
        if(username.length() > 0)
            mqttClient.setUsernamePassword(username, password);

        // Set the keepalive time
        mqttClient.setKeepAliveInterval(keep_alive);

        int retryAttempts = 0;

        while(!mqttClient.connected() && retryAttempts < 5)
        {
            printModuleName(); Serial.println("Attempting to connect to broker: " + address + ":" + String(port));

            // Attempt to Connect to the MQTT client 
            if(!mqttClient.connect(address.c_str(), port)){
                printModuleName(); Serial.println("Failed to connect to broker: " + getMQTTError());

                
            }

            // If our retry limit has been reached we dont want to try to send data cause it wont work
            if(retryAttempts == 4){
                printModuleName(); Serial.println("Retry limit exceeded!");
                return;
            }

            retryAttempts++;
        }
        
        printModuleName(); Serial.println("Successfully connected to broker!");
        printModuleName(); Serial.println("Attempting to send data...");

        // Tell the broker we are still here
        mqttClient.poll();

        // Start a message write the data and close the message
        if(mqttClient.beginMessage(topic, false, 2) != 1){
            printModuleName(); Serial.println("Failed to begin message!");
        }
        mqttClient.print(manInst->getJSONString());

        // Check to see if we are actually closing messages properly
        if(mqttClient.endMessage() != 1){
            printModuleName(); Serial.println("Failed to close message!");
        }
        else{
            printModuleName(); Serial.println("Data has been successfully sent!");
        }

            
    }
    else{
        printModuleName(); Serial.println("Module not initialized! If using credentials from SD make sure they are loaded first.");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MQTT::publish(Loom_BatchSD& batchSD){
    if(moduleInitialized ){
        if(batchSD.shouldPublish()){
            // If we are logging in using credentials then supply them
            if(username.length() > 0)
                mqttClient.setUsernamePassword(username, password);

            // Set the keepalive time
            mqttClient.setKeepAliveInterval(keep_alive);

            int retryAttempts = 0;

            while(!mqttClient.connected() && retryAttempts < 5)
            {
                printModuleName(); Serial.println("Attempting to connect to broker: " + address + ":" + String(port));

                // Attempt to Connect to the MQTT client 
                if(!mqttClient.connect(address.c_str(), port)){
                    printModuleName(); Serial.println("Failed to connect to broker: " + getMQTTError());

                    
                }

                // If our retry limit has been reached we dont want to try to send data cause it wont work
                if(retryAttempts == 4){
                    printModuleName(); Serial.println("Retry limit exceeded!");
                    return;
                }

                retryAttempts++;
            }
            
            printModuleName(); Serial.println("Successfully connected to broker!");
            printModuleName(); Serial.println("Attempting to send data...");

            // Tell the broker we are still here
            mqttClient.poll();
            
            std::vector<String> batch = batchSD.getBatch();
            for(int i = 0; i < batch.size(); i++){
                printModuleName(); Serial.println("Publishing Packet " + String(i+1) + " of " + String(batch.size()));
                
                // Start a message write the data and close the message
                mqttClient.beginMessage(topic);
                mqttClient.print(batch[i]);
                mqttClient.endMessage();
            }
            
            printModuleName(); Serial.println("Data has been successfully sent!");
            
        }
        else{
            printModuleName(); Serial.println("Batch not ready to publish: " + String(batchSD.getCurrentBatch()) + "/" + String(batchSD.getBatchSize()));
        }
    }
    else{
        printModuleName(); Serial.println("Module not initialized! If using credentials from SD make sure they are loaded first.");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
String Loom_MQTT::getMQTTError(){
    // Convert error codes to actual descriptions
    switch(mqttClient.connectError()){
        case -2:
            return String("CONNECTION_REFUSED");
        case -1:
            return String("CONNECTION_TIMEOUT");
        case 1:
            return String("UNACCEPTABLE_PROTOCOL_VERSION");
        case 2:
            return String("IDENTIFIER_REJECTED");
        case 3:
            return String("SERVER_UNAVAILABLE");
        case 4:
            return String("BAD_USER_NAME_OR_PASSWORD");
        case 5:
            return String("NOT_AUTHORIZED");
        default:
            return String("UNKNOWN_ERROR");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MQTT::loadConfigFromJSON(String json){

    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    DeserializationError deserialError = deserializeJson(doc, json);

    // Check if an error occurred and if so print it
    if(deserialError != DeserializationError::Ok){
        printModuleName(); Serial.println("There was an error reading the MQTT credentials from SD: " + String(deserialError.c_str()));
    }
    
    address = doc["broker"].as<String>();
    port = doc["port"].as<int>();
    database_name = doc["database"].as<String>();
    username = doc["username"].as<String>();
    password = doc["password"].as<String>();

     // Formulate a topic to publish on with the format "DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chime1
    topic = database_name + "/" + (manInst->get_device_name() + String(manInst->get_instance_num()));
    moduleInitialized = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////