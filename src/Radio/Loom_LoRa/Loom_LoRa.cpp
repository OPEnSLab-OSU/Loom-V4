#include "Loom_LoRa.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LoRa::Loom_LoRa(
        Manager& man,
        const int address, 
        const uint8_t powerLevel, 
        const uint8_t retryCount, 
        const uint16_t retryTimeout,
        const uint16_t max_message_len
    ) : Radio("LoRa"), manInst(&man), driver{RFM95_CS, RFM95_INT}
    {
        // If an address was not set manually use the instance number
        if(address == -1){ this->deviceAddress = manInst->get_instance_num(); }
        else{ this->deviceAddress = address; }
    
        manager = new RHReliableDatagram(driver, this->deviceAddress);
        this->powerLevel = powerLevel;
        this->retryCount = retryCount;
        this->retryTimeout = retryTimeout;
        this->maxMessageLength = max_message_len;
        manInst->registerModule(this);
    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::initialize(){
    // Set CS pin as pull up
    pinMode(RFM95_CS, INPUT_PULLUP);
    
    // Reset the radio
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);

    // Initialize the radio manager
    if(manager->init()){
        printModuleName("Radio manager successfully initialized!");
    }
    else{
        printModuleName("Radio manager failed to initialize!");
        moduleInitialized = false;
        return;
    }

    // Set the radio frequency
    if(driver.setFrequency(RF95_FREQ)){
        printModuleName("Radio frequency successfully set to: " + String(RF95_FREQ));
    }
    else{
        printModuleName("Failed to set frequency!");
        moduleInitialized = false;
        return;
    }

    // Set radio power level
    printModuleName("Setting device power level to: " + String(powerLevel));
    driver.setTxPower(powerLevel, false);

    // Set timeout time
    printModuleName("Timeout time set to: " + String(retryTimeout));
    manager->setTimeout(retryTimeout);

    // Set retry attempts
    printModuleName("Retry count set to: " + String(retryCount));
    manager->setRetries(retryCount);

    // Print the set address of the device
    printModuleName("Address set to: " + String(manager->thisAddress()));

    /* 
        
        https://cdn.sparkfun.com/assets/a/e/7/e/b/RFM95_96_97_98W.pdf, Page 22
    */

    // Set bandwidth
    driver.setSignalBandwidth(125000);

    // Higher spreading factors give us more range
    driver.setSpreadingFactor(10); 

    // Coding rate should be 4/5
	driver.setCodingRate4(8);	
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
bool Loom_LoRa::receive(uint maxWaitTime){
    if(moduleInitialized){
        bool recvStatus = false;
        uint8_t fromAddress;
        
        // Write all null bytes to the buffer
        uint8_t buffer[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buffer);

        printModuleName("Waiting for packet...");

        // Non-blocking receive if time is set to 0
        if(maxWaitTime == 0){
            recvStatus = manager->recvfromAck(buffer, &len, &fromAddress);
        }
        else{
            recvStatus = manager->recvfromAckTimeout(buffer, &len, maxWaitTime, &fromAddress);
        }

        // If a packet was received 
        if(recvStatus){
            printModuleName("Packet Received!");
            signalStrength = driver.lastRssi();
            recvStatus = bufferToJson(buffer, manInst->getDocument());

            // Update device details
            if(recvStatus){
                manInst->set_device_name(manInst->getDocument()["id"]["name"].as<String>());
                manInst->set_instance_num(manInst->getDocument()["id"]["instance"].as<int>());
            }

            // Check if we have a numPackets field which tells us we should expect more packets
            if(!manInst->getDocument()["numPackets"].isNull()){
                receivePartial(maxWaitTime);
            }
        }
        else{
            printModuleName("No Packet Received");
        }

        driver.sleep();
        return recvStatus;
    }else{
        printModuleName("Module not initialized!");
        return false;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receivePartial(uint waitTime){
    if(moduleInitialized){
        bool recvStatus = false;

        uint8_t fromAddress;

        // Write all null bytes to the buffer
        uint8_t buffer[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buffer);
       
        // Contents array to store each module in
        JsonArray contents = manInst->getDocument().createNestedArray("contents");
        
        // Gets the number of additional packets the hub should expect
        int numPackets = manInst->getDocument()["numPackets"].as<int>();

        // Loop for the given number of packets we are expecting
        for(int i = 0; i < numPackets; i++){
            tempDoc.clear();
            printModuleName("Waiting for packet " + String(i+1) + " / " + String(numPackets));

            // Non-blocking receive if time is set to 0
            if(waitTime == 0){
                recvStatus = manager->recvfromAck(buffer, &len, &fromAddress);
            }
            else{
                recvStatus = manager->recvfromAckTimeout(buffer, &len, waitTime, &fromAddress);
            }

            for(int i = 0; i < 255; i++){
                Serial.print((char)buffer[i]);
                Serial.print(" ");
            }

            // If a packet was received 
            if(recvStatus){
                printModuleName("Fragment received " + String(i+1) + " / " + String(numPackets));
                signalStrength = driver.lastRssi();
                recvStatus = bufferToJson(buffer, tempDoc);

                // Add the current module to the overall contents array
                contents.add(tempDoc["contents"][0].as<JsonObject>());
            }
            else{
                printModuleName("No Packet Received");
            }
        }

        driver.sleep();
        return recvStatus;
    }else{
        printModuleName("Module not initialized!");
        return false;
    }
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
        printModuleName("Module not initialized!");
        return false;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendFull(const uint8_t destinationAddress){
    char buffer[maxMessageLength];

    // Try to write the JSON to the buffer
    if(!jsonToBuffer(buffer, manInst->getDocument().as<JsonObject>())){
        printModuleName("Failed to convert JSON to MsgPack");
        return false;
    }

    if(!manager->sendtoWait((uint8_t*)buffer, sizeof(buffer), destinationAddress)){
        printModuleName("Failed to send packet to specified address! The message may have gotten there but not received and acknowledgement response");
    }else{
        printModuleName("Successfully transmitted packet!");
    }

    signalStrength = driver.lastRssi();
    driver.sleep();
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendPartial(const uint8_t destinationAddress){
    printModuleName("Packet was greater than the maximum packet length the packet will be fragmented");
    
    // Disable retries when sending partial packets
    manager->setRetries(0);

    char buffer[maxMessageLength]; 
    // Get a clear JsonObject reference of the tempDoc
    JsonObject obj = tempDoc.to<JsonObject>();
    obj["type"] = manInst->getDocument()["type"].as<String>();
    JsonObject objID = obj.createNestedObject("id");
    

    // Gets the number of additional packets the hub should expect
    int numPackets = manInst->getDocument()["contents"].size();

    // Re-construct the packet header including the number of packets to expect after this initial one
    objID["name"] = manInst->getDocument()["id"]["name"].as<String>();   
    objID["instance"] = manInst->getDocument()["id"]["instance"].as<int>();  
    obj["numPackets"] = numPackets;
    
    // Try to write the JSON to the buffer
    if(!jsonToBuffer(buffer, obj)){
        printModuleName("Failed to convert JSON to MsgPack");
        return false;
    }

    for(int i = 0; i < 255; i++){
        Serial.print((uint8_t)buffer[i], HEX);
        Serial.print(" ");
    }

    // Send the packet off
    if(!manager->sendtoWait((uint8_t*)buffer, measureMsgPack(obj), destinationAddress)){
        printModuleName("Failed to send packet to specified address! The message may have gotten their but not received and acknowledgement response");
    }else{
        printModuleName("Successfully transmitted packet!");
    }

    // Send all modules by themselves to allow for larger amounts of data to be sent over radio
    sendModules(manInst->getDocument().as<JsonObject>(), destinationAddress);

    // If we have a timestamp we also need to copy this across to the new one, MUST BE BEFORE
    if(!manInst->getDocument()["timestamp"].isNull()){
        JsonObject objTS = obj.createNestedObject("timestamp");
        objTS["time_utc"] = manInst->getDocument()["timestamp"]["time_utc"].as<String>();
        objTS["time_local"] = manInst->getDocument()["timestamp"]["time_local"].as<String>();
    }

    
    signalStrength = driver.lastRssi();
    manager->setRetries(3);
    driver.sleep();
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendModules(JsonObject json, const uint8_t destinationAddress){
    
    char buffer[maxMessageLength];  

    // Use as with a clear to avoid dangling roots
    tempDoc.clear();
    int numPackets = json["contents"].size();

    // Loop through the number of packets we need to send
    for(int i = 0; i < numPackets; i++){
        JsonObject obj = tempDoc.to<JsonObject>();
        JsonArray objContents = obj.createNestedArray("contents");

        // Create a data object for each content
        objContents[0]["module"] = json["contents"][i]["module"];
        JsonObject objData = objContents[0].createNestedObject("data");
        
        // Get each piece of data that the module had
        JsonObject old_data = json["contents"][i]["data"];
	    for (JsonPair kv : old_data){
		    objData[kv.key()] = kv.value();
	    }

        // Try to write the JSON to the buffer
        if(!jsonToBuffer(buffer, obj)){
            printModuleName("Failed to convert JSON to MsgPack");
            return false;
        }

        for(int i = 0; i < 255; i++){
            Serial.print((uint8_t)buffer[i], HEX);
            Serial.print(" ");
        }

        serializeJsonPretty(obj, Serial);

        // Send the packet off
        if(!manager->sendtoWait((uint8_t*)buffer, measureMsgPack(obj), destinationAddress)){
            printModuleName("Failed to send packet to specified address! The message may have gotten their but not received and acknowledgement response");
        }else{
            printModuleName("Successfully transmitted packet!");
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
