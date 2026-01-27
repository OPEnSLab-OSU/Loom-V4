#include "Loom_MongoDB.h"
#include "Logger.h"
#include "../../../Sensors/Loom_Analog/Loom_Analog.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MongoDB::Loom_MongoDB(
                    Manager& man,
                    NetworkComponent& internet_client, 
                    const char* broker_address, 
                    int broker_port, 
                    const char* database_name, 
                    const char* broker_user, 
                    const char* broker_pass,
                    const char* projectServer
                ) : MQTTComponent("MongoDB", internet_client),
                    manInst(&man)
                    {
                        /* MQTT Connection parameters */
                        strncpy(this->address, broker_address, 100);
                        port = broker_port;
                        strncpy(this->username, broker_user, 100);
                        strncpy(this->password, broker_pass, 100);

                        /* Local MongoDB parameters */
                        strncpy(this->database_name, database_name, 100);
                        strncpy(this->projectServer, projectServer, 100);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MongoDB::Loom_MongoDB(Manager& man, NetworkComponent& internet_client) : MQTTComponent("MongoDB", internet_client), manInst(&man){
    moduleInitialized = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::publish(){
    FUNCTION_START;
    
    char jsonString[MAX_JSON_SIZE];
    if(moduleInitialized){

        TIMER_DISABLE;
        
        if(strlen(projectServer) > 0)
            // Formulate a topic to publish on with the format "ProjectName/DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chimes/Chime1
            snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s/%s%i"), projectServer, database_name, manInst->get_device_name(), manInst->get_instance_num());
        else
            // Formulate a topic to publish on with the format "DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chime1
            snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s%i"), database_name, manInst->get_device_name(), manInst->get_instance_num());

        /* Attempt to connect to the broker if it fails we should just return */
        if(!connectToBroker()){
            FUNCTION_END;
            return false;
        }

        /* Attempt to publish the data to the given topic */
        manInst->getJSONString(jsonString);
        if(!publishMessage(topic, jsonString)){
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

bool Loom_MongoDB::publish(Loom_BatchSD& batchSD){
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    // 1. Battery Check
    if(Loom_Analog::getBatteryVoltage() < 3.4){
        WARNING(F("Module not initialized! Battery doesn't have enough power."));
        FUNCTION_END;
        return false;    
    }
    
    // 2. Define Buffers (Static to prevent Stack Overflow)
    // chunkBuffer: Holds the big group of packets to send (e.g., "[{...},{...},{...}]")
    static char chunkBuffer[MAX_JSON_SIZE]; 
    // packetBuffer: Holds the single line we are currently reading
    static char packetBuffer[512]; 

    int chunkCount = 0;       // How many packets are currently in the chunk
    int packetIndex = 0;      // Current position in the packetBuffer
    int packetNumber = 0;     // Total progress tracking
    char c;

    if(moduleInitialized){
        TIMER_DISABLE;
        if(batchSD.shouldPublish()){

            // Setup Topic
            if(strlen(projectServer) > 0)
                snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s%i"), database_name, manInst->get_device_name(), manInst->get_instance_num());
            else
                snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s%i"), database_name, manInst->get_device_name(), manInst->get_instance_num());
            
            if(!connectToBroker()) return false;
            
            File fileOutput = batchSD.getBatch();
            bool allDataSuccess = true;
            
            // Initialize the Chunk Buffer with starting bracket
            strcpy(chunkBuffer, "["); 
            
            // ========================= READ LOOP =========================
            while(fileOutput.available()){
                c = fileOutput.read();

                // Check for line endings
                if(c == '\r' || c == '\n'){
                    if(packetIndex == 0) continue; // Skip empty lines
                    
                    packetBuffer[packetIndex] = '\0'; // Terminate current packet

                    // --- CHECK: Will adding this packet overflow the Chunk Buffer? ---
                    // Current Chunk + Comma + New Packet + Closing Bracket + Null Term
                    if(strlen(chunkBuffer) + strlen(packetBuffer) + 3 >= MAX_JSON_SIZE) {
                        // BUFFER FULL! Send what we have immediately.
                        strcat(chunkBuffer, "]"); // Close the array
                        
                        // Send the current full chunk
                        if(!sendBatch(chunkBuffer, packetNumber, batchSD.getBatchSize())) {
                            allDataSuccess = false;
                        }

                        // Reset Chunk Buffer for the new packet
                        strcpy(chunkBuffer, "[");
                        chunkCount = 0;
                    }

                    // --- ADD PACKET TO CHUNK ---
                    if(chunkCount > 0) {
                        strcat(chunkBuffer, ","); // Add comma if not first item
                    }
                    strcat(chunkBuffer, packetBuffer);
                    chunkCount++;
                    packetNumber++;

                    // --- CHECK: Have we reached the group limit (5)? ---
                    if(chunkCount >= 5) {
                        strcat(chunkBuffer, "]"); // Close the array
                        
                        // Send the chunk
                        if(!sendBatch(chunkBuffer, packetNumber, batchSD.getBatchSize())) {
                            allDataSuccess = false;
                        }

                        // Reset Chunk Buffer
                        strcpy(chunkBuffer, "[");
                        chunkCount = 0;
                        
                        // Moderate delay to let the modem breathe (Traffic Cop)
                        delay(250); 
                    }
                    
                    // Reset Packet Buffer for next line
                    packetIndex = 0;
                } 
                else {
                    // Safety: Read into packetBuffer
                    if(packetIndex < 512 - 1) {
                        packetBuffer[packetIndex] = c;
                        packetIndex++;
                    }
                }
            } 
            // ======================= END READ LOOP =======================

            fileOutput.close();

            // --- FLUSH: Send any remaining packets in the buffer ---
            if(chunkCount > 0) {
                strcat(chunkBuffer, "]"); // Close the array
                if(!sendBatch(chunkBuffer, packetNumber, batchSD.getBatchSize())) {
                    allDataSuccess = false;
                }
            }
            
            if(allDataSuccess) LOG(F("Batch upload complete!")); 
            else {
                WARNING(F("Some batches failed to send."));
                return false;
            }
        }
    }
    return true;
}

// -----------------------------------------------------------------------------------
// HELPER FUNCTION: Handles the Retry Logic & Sending
// Paste this ABOVE the publish() function or inside the class as a private method
// -----------------------------------------------------------------------------------
bool Loom_MongoDB::sendBatch(char* payload, int progress, int total) {
    char output[OUTPUT_SIZE];
    bool sent = false;

    // Try 3 times
    for(int i=0; i<3; i++) {
        Watchdog.reset(); // Keep device alive

        // Reconnect if needed
        if(!mqttClient.connected()) {
            connectToBroker();
            delay(500);
        }

        snprintf_P(output, OUTPUT_SIZE, PSTR("Sending Batch (Progress: %i/%i)... Try %i"), progress, total, i+1);
        LOG(output);

        // Send with QoS 0 (Fastest) or 1 (Reliable). 
        // Using QoS 0 here because we have retry logic.
        if(publishMessage(topic, payload, false, 0)) {
            sent = true;
            break;
        } else {
            delay(1000); // Wait before retry
        }
    }

    if(!sent) {
        ERROR(F("Failed to send batch after 3 attempts."));
        return false;
    }
    return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MongoDB::loadConfigFromJSON(char* json){
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    char topic[MAX_TOPIC_LENGTH];

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
    memset(database_name, '\0', 100);
    memset(username, '\0', 100);
    memset(password, '\0', 100);
    port = 0;
    
    /* We should check if any parameter is null */
    if(!doc["broker"].isNull())
        strncpy(address, doc["broker"].as<const char*>(), 100);

    if(!doc["database"].isNull())
        strncpy(database_name, doc["database"].as<const char*>(), 100);
    
    if(!doc["username"].isNull())
        strncpy(username, doc["username"].as<const char*>(), 100);

    if(!doc["password"].isNull())
        strncpy(password, doc["password"].as<const char*>(), 100);

    if(!doc["project"].isNull())
        strncpy(projectServer, doc["project"].as<const char*>(), 100);

    if(!doc["port"].isNull())
        port = doc["port"].as<int>();
   
    if(strlen(projectServer) > 0){
        // Formulate a topic to publish on with the format "ProjectName/DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chimes/Chime1
        snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s/%s%i"), projectServer, database_name, manInst->get_device_name(), manInst->get_instance_num());
    }
    else{
        // Formulate a topic to publish on with the format "DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chime1
        snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s%i"), database_name, manInst->get_device_name(), manInst->get_instance_num());
    }
    
    moduleInitialized = true;
    free(json);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////