#include "Loom_LoRa.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LoRa::Loom_LoRa(
        Manager& man,
        const LORA_RANGE range,
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
        this->range = range;
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
        snprintf(output, OUTPUT_SIZE, "Radio frequency successfully set to: %f", RF95_FREQ);
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
    
    if(range == LORA_RANGE::SHORT){
        // Higher spreading factors give us more range
        driver.setSpreadingFactor(7); 

        // Coding rate should be 4/5
        driver.setCodingRate4(5);	
    }else{
        // Set the driver to use long range configuration with a spreading factor of 12 and a coding rate of 4/8
        driver.setModemConfig(RH_RF95::ModemConfigChoice::Bw125Cr48Sf4096);
        driver.setSpreadingFactor(12);
        driver.setCodingRate4(8);
        manager->setTimeout(60000);
    }

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
bool Loom_LoRa::send(const uint8_t destinationAddress){
    if(moduleInitialized){

        // If the message we are trying to send is greater than the max size we need to fragment the packet
        if(measureMsgPack(manInst->getDocument()) > maxMessageLength){
            return sendPartial(destinationAddress);
        }else{
            return sendFull(destinationAddress);
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
        
        /* If we are using long range mode with a wait of less than 50 seconds this may not work so we should tell the user*/
        if(range == LORA_RANGE::LONG && maxWaitTime < 50000){
            WARNING("Using long range mode with a timeout time less than 13 seconds can lead to weird behavior where the packet may not be completely received.");
        }

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
bool Loom_LoRa::sendFull(const uint8_t destinationAddress){
    return transmit(manInst->getDocument().as<JsonObject>(), destinationAddress);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendPartial(const uint8_t destinationAddress){
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
    sendDoc["type"] = manInst->getDocument()["type"].as<const char*>();

    // Re-construct the packet header including the number of packets to expect after this initial one
    JsonObject objID = sendDoc.createNestedObject("id");
    objID["name"] = manInst->getDocument()["id"]["name"].as<const char*>();   
    objID["instance"] = manInst->getDocument()["id"]["instance"].as<int>();

    // Gets the number of additional packets the hub should expect
    int numPackets = manInst->getDocument()["contents"].size();
    sendDoc["numPackets"] = numPackets;

    // Create an empty contents array to preserve formatting
    sendDoc.createNestedArray("contents");

    // If we have a timestamp we also need to copy this across to the new one, MUST BE BEFORE
    if(!manInst->getDocument()["timestamp"].isNull()){
        JsonObject objTS = sendDoc.createNestedObject("timestamp");
        objTS["time_utc"] = manInst->getDocument()["timestamp"]["time_utc"].as<const char*>();
        objTS["time_local"] = manInst->getDocument()["timestamp"]["time_local"].as<const char*>();
    }
   
    if(!transmit(sendDoc.as<JsonObject>(), destinationAddress)){
        ERROR(F("Unable to transmit initial packet fragmentation notice! Split packets will not be sent"));
        return false;
    }

    // Send all modules by themselves to allow for larger amounts of data to be sent over radio
    return sendModules(manInst->getDocument().as<JsonObject>(), numPackets, destinationAddress);;
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
    driver.available();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::power_down(){
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