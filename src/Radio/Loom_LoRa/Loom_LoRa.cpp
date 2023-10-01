#include "Loom_LoRa.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LoRa::Loom_LoRa(
        Manager& man,
        const int address, 
        const uint8_t powerLevel, 
        const uint8_t retryCount, 
        const uint16_t retryTimeout
    ) : Radio("LoRa"), manInst(&man), driver{RFM95_CS, RFM95_INT}
    {
        // If an address was not set manually use the instance number
        if(address == -1){ this->deviceAddress = manInst->get_instance_num(); }
        else{ this->deviceAddress = address; }
    
        manager = new RHReliableDatagram(driver, this->deviceAddress);
        this->powerLevel = powerLevel;
        this->retryCount = retryCount;
        this->retryTimeout = retryTimeout;
        this->maxMessageLength = RH_RF95_MAX_MESSAGE_LEN;
        manInst->registerModule(this);
    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::initialize(){
    char output[OUTPUT_SIZE];
    // Set CS pin as pull up
    pinMode(RFM95_CS, INPUT_PULLUP);
    
    // Reset the radio
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);

    // Initialize the radio manager
    if(manager->init()){
        LOG(F("Radio manager successfully initialized!"));
    }
    else{
        ERROR(F("Radio manager failed to initialize!"));
        moduleInitialized = false;
        return;
    }

    // Set the radio frequency
    if(driver.setFrequency(RF95_FREQ)){
        snprintf(output, OUTPUT_SIZE, "Radio frequency successfully set to: %i", RF95_FREQ);
        LOG(output);
    }
    else{
        ERROR(F("Failed to set frequency!"));
        moduleInitialized = false;
        return;
    }

    // Set radio power level
    snprintf(output, OUTPUT_SIZE, "Setting device power level to: %i", powerLevel);
    LOG(output);
    driver.setTxPower(powerLevel, false);

    // Set timeout time
    snprintf(output, OUTPUT_SIZE, "Timeout time set to: %i,", retryTimeout);
    LOG(output);
    manager->setTimeout(retryTimeout);

    // Set retry attempts
    snprintf(output, OUTPUT_SIZE, "Retry count set to: %i", retryCount);
    LOG(output);
    manager->setRetries(retryCount);

    // Print the set address of the device
    snprintf(output, OUTPUT_SIZE, "Address set to: %i", manager->thisAddress());
    LOG(output);
    
    /* 
        https://cdn.sparkfun.com/assets/a/e/7/e/b/RFM95_96_97_98W.pdf, Page 22
    */

    // Set bandwidth
    driver.setSignalBandwidth(125000);

    // Higher spreading factors give us more range
    driver.setSpreadingFactor(7); 

    // Coding rate should be 4/5
	driver.setCodingRate4(5);	
	driver.sleep();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::package(){
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["RSSI"] = getSignalStrength();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::setAddress(uint8_t addr){
    deviceAddress = addr;
    manager->setThisAddress(addr);
    driver.sleep();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::transmit(JsonObject json, int destination){
    // Buffer of the data being sent
    bool returnState = false;
    uint8_t buffer[maxMessageLength];
    memset(buffer, '\0', maxMessageLength);

    // Try to write the JSON to the buffer
    if(!jsonToBuffer(buffer, json)){
        ERROR(F("Failed to convert JSON to MsgPack"));
        return false;
    }

    if(!manager->sendtoWait(buffer, sizeof(buffer), destination)){
        ERROR(F("Failed to send packet to specified address!"));

    }else{
        LOG(F("Successfully transmitted packet!"));
        returnState = true;
    }

    signalStrength = driver.lastRssi();
    driver.sleep();
    return returnState;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::recv(int waitTime){
    bool recvStatus = false;
    uint8_t fromAddress;
    
    uint8_t buffer[maxMessageLength];
    memset(buffer, '\0', RECV_DATA_SIZE); // Write all null bytes to the buffer
    uint8_t len = sizeof(buffer);

    LOG(F("Waiting for packet..."));

    // Non-blocking receive if time is set to 0
    if(waitTime == 0){
        recvStatus = manager->recvfromAck(buffer, &len, &fromAddress);
    }
    else{
        recvStatus = manager->recvfromAckTimeout(buffer, &len, waitTime, &fromAddress);
        
    }


    // If a packet was received 
    if(recvStatus){
        signalStrength = driver.lastRssi();
        recvStatus = bufferToJson(buffer);
        memset(recvData, '\0', RECV_DATA_SIZE);
        serializeJson(recvDoc, recvData, RECV_DATA_SIZE);
    }
    else{
        WARNING(F("No Packet Received"));
    }
    driver.sleep();
    return recvStatus;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::send(const uint8_t destinationAddress){
    return send(destinationAddress, manInst->getDocument().as<JsonObject>());
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::send(const uint8_t destinationAddress, JsonObject json){
    if(moduleInitialized){
        // If the message we are trying to send is greater than the max size we need to fragment the packet
        if(measureMsgPack(manInst->getDocument()) > maxMessageLength){
            return sendPartial(destinationAddress, json);
        }else{
            return sendFull(destinationAddress, json);
        }
    }
    else{
        ERROR(F("Module not initialized!"));
        return false;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendBatch(const uint8_t destinationAddress){
    char output[OUTPUT_SIZE];
    if(moduleInitialized){
        // Check if we are actually ready to publish the batch of data 
        if(batchSD->shouldPublish()){
            /* 
                Send the notification to the other radio to tell it to prepare to expect a batch of data, the packet is formatted as follows:
                {
                    "batch_size": <size>
                }
            */
            StaticJsonDocument<100> batchNotify;
            char packet[2000]; // Buffer to read the stored batch packet into
            int packetNumber = 0;
            int index = 0;
            char c;
            batchNotify["batch_size"] = batchSD->getBatchSize();

            /* We must successfully send the first packet before sending consecutive ones */
            if(send(destinationAddress, batchNotify.as<JsonObject>())){

                /* Get the batch file stream to read data from */
                File fileOutput = batchSD->getBatch();

                // Read all available data from the batch file one line at a time into the buffer 
                while(fileOutput.available()){
                    c = fileOutput.read();
                    if(c == '\r'){
                        snprintf_P(output, OUTPUT_SIZE, PSTR("Transmitting Packet %i of %d"), packetNumber+1, batchSD->getBatchSize());
                        LOG(output);
                        packet[index] = '\0';
                        // Use the main document to convert the string into a JSON document 
                        deserializeJson(manInst->getDocument(), (const char*)packet, 2000);

                        // Transmit the document to the remote device 
                        if(send(destinationAddress, manInst->getDocument().as<JsonObject>())){
                            snprintf_P(output, OUTPUT_SIZE, PSTR("Successfully transmitted packet %i of %d"), packetNumber+1, batchSD->getBatchSize());
                            LOG(output);
                        }
                        else{
                            snprintf_P(output, OUTPUT_SIZE, PSTR("Failed to transmit packet %i of %d"), packetNumber+1, batchSD->getBatchSize());
                            ERROR(output);
                        }

                        delay(500);
                        index = 0;
                        packetNumber++;
                    }
                    else{
                        packet[index] = c;
                        index++;
                    }  
                    
                }
                fileOutput.close();
            }
        }
    }
    else{
        ERROR(F("Module not initialized!"));
        return false;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receive(uint maxWaitTime){
    if(moduleInitialized){

        // Wait for packet to arrive
        if(recv(maxWaitTime)){
            LOG(F("Packet Received!"));


            manInst->set_device_name(recvDoc["id"]["name"].as<const char*>());
            manInst->set_instance_num(recvDoc["id"]["instance"].as<int>());
            deserializeJson(manInst->getDocument(), (const char*)recvData, RECV_DATA_SIZE); //must cast to const char* to avoid no-copy behavior
        
            // Check if we have a numPackets field which tells us we should expect more packets
            if(!manInst->getDocument()["numPackets"].isNull()){
                receivePartial(maxWaitTime);
                manInst->getDocument().remove("numPackets");
            }

            if(manInst->getDocument()["id"].isNull()){
                return false;
            }

            return true;
        }
        else{
            return false;
        }

    }else{
        ERROR(F("Module not initialized!"));
        return false;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receiveBatch(uint maxWaitTime, int* numberOfPackets){
    char output[OUTPUT_SIZE];
    if(moduleInitialized){
        // Wait for packet to arrive
        if(recv(maxWaitTime)){
            LOG(F("Packet Received!"));

            // Get the number of additional packets to expect
            if(!recvDoc["batch_size"].isNull()){
                *numberOfPackets = recvDoc["batch_size"].as<int>();
                return true;
            }
            else{
                manInst->set_device_name(recvDoc["id"]["name"].as<const char*>());
                manInst->set_instance_num(recvDoc["id"]["instance"].as<int>());
                deserializeJson(manInst->getDocument(), (const char*)recvData, RECV_DATA_SIZE); //must cast to const char* to avoid no-copy behavior
            
                // Check if we have a numPackets field which tells us we should expect more packets
                if(!manInst->getDocument()["numPackets"].isNull()){
                    receivePartial(maxWaitTime);
                    manInst->getDocument().remove("numPackets");
                }

                // If there is no "id" in the packet something got messed up in transmission
                if(manInst->getDocument()["id"].isNull()){
                    snprintf_P(output, OUTPUT_SIZE, PSTR("Failed to receive packet!"));
                    ERROR(output);
                    return false;
                }else{
                    snprintf_P(output, OUTPUT_SIZE, PSTR("Received packet!"));
                    LOG(output);
                    *numberOfPackets--;
                    return true;
                }
            }
           
        }
        else{
            return false;
        }

    }else{
        ERROR(F("Module not initialized!"));
        return false;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receivePartial(uint waitTime){
    char output[OUTPUT_SIZE];

    if(moduleInitialized){
        // Gets the number of additional packets the hub should expect
        int numPackets = manInst->getDocument()["numPackets"].as<int>();
        JsonArray contents = manInst->getDocument()["contents"].as<JsonArray>();
        StaticJsonDocument<300> tempDoc;


        // Loop for the given number of packets we are expecting
        for(int i = 0; i < numPackets; i++){
            snprintf(output, OUTPUT_SIZE, "Waiting for packet %i / %i",i+1, numPackets);
            LOG(output);

            // If a packet was received 
            if(recv(waitTime)){
                snprintf(output, OUTPUT_SIZE, "Fragment received %i / %i", i+1, numPackets);
                LOG(output);
                
                deserializeJson(tempDoc, (const char*)recvData, RECV_DATA_SIZE); //must cast to const char* to avoid no-copy behavior
                // Add the current module to the overall contents array
                contents.add(tempDoc);
            }
            else{
                ERROR(F("No Packet Received"));
            }
        }

        return true;
    }else{
        ERROR(F("Module not initialized!"));
        return false;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendFull(const uint8_t destinationAddress, JsonObject json){
    return transmit(json, destinationAddress);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendPartial(const uint8_t destinationAddress, JsonObject json){
    LOG(F("Packet was greater than the maximum packet length the packet will be fragmented"));
    sendDoc.clear();
   
   /* 
    This is what the initial packet will look like
    {
        "type": "data",
        "id": {
            "name": "Dend4",
            "instance": 123
        },
        "numPackets": 2
        "contents": [],
        "timestamp": {
            "time_utc": "2022-11-30T6:48:21Z",
            "time_local": "2022-11-29T22:48:21Z"
        }
    }
   */
    sendDoc["type"] = json["type"].as<const char*>();

    // Re-construct the packet header including the number of packets to expect after this initial one
    JsonObject objID = sendDoc.createNestedObject("id");
    objID["name"] = json["id"]["name"].as<const char*>();   
    objID["instance"] = json["id"]["instance"].as<int>();

    // Gets the number of additional packets the hub should expect
    int numPackets = json["contents"].size();
    sendDoc["numPackets"] = numPackets;

    // Create an empty contents array to preserve formatting
    sendDoc.createNestedArray("contents");

    // If we have a timestamp we also need to copy this across to the new one, MUST BE BEFORE
    if(!json["timestamp"].isNull()){
        JsonObject objTS = sendDoc.createNestedObject("timestamp");
        objTS["time_utc"] = json["timestamp"]["time_utc"].as<const char*>();
        objTS["time_local"] = json["timestamp"]["time_local"].as<const char*>();
    }
   
    if(!transmit(sendDoc.as<JsonObject>(), destinationAddress)){
        ERROR(F("Unable to transmit initial packet fragmentation notice! Split packets will not be sent"));
        return false;
    }

    // Send all modules by themselves to allow for larger amounts of data to be sent over radio
    return sendModules(json, numPackets, destinationAddress);;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendModules(JsonObject json, int numModules, const uint8_t destinationAddress){
    char output[OUTPUT_SIZE];
    /*
        This is how each module will be sent across
        {
            "module": "SHT31",
            "data": {
                "Temperature": 21.35000038,
                "Humidity": 53.22999954
            }
        }
    */

    // Loop through the number of packets we need to send
    for(int i = 0; i < numModules; i++){
        sendDoc.clear();

        // Set the module key to whatever the main one is
        JsonArray contents = manInst->getDocument()["contents"].as<JsonArray>();
        sendDoc.set(contents[i].as<JsonObject>());

        snprintf(output, OUTPUT_SIZE, "Fragmented Packet Being Sent (%i/%i)", i+1, numModules);
        LOG(output);
        

        // Attempt to transmit the document to the other device
        if(!transmit(sendDoc.as<JsonObject>(), destinationAddress)){
            ERROR(F("Failed to transmit fragmented packet!"));
        }
        delay(500);
    }
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::power_up(){
    // If we are using batchSD then we should check if we are on the power up of a cycle where we are going to be publishing 
    if(batchSD != nullptr){
        poweredUp = (batchSD->getCurrentBatch() == batchSD->getBatchSize()-1);
    }

    // If we are going to publish this cycle we should power up the driver
    if(poweredUp){
        driver.available();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::power_down(){

    // If we did power on this cycle we should go back to sleep
    if(poweredUp){
        driver.sleep();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
