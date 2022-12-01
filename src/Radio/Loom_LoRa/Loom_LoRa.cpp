#include "Loom_LoRa.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LoRa::Loom_LoRa(
        Manager& man,
        const uint8_t address, 
        const uint8_t powerLevel, 
        const uint8_t retryCount, 
        const uint16_t retryTimeout,
        const uint16_t max_message_len
    ) : Radio("LoRa"), manInst(&man), driver{RFM95_CS, RFM95_INT}
    {
        // Pull the instance number from the manager
        this->deviceAddress = address;
    
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
        printModuleName(); Serial.println("Radio manager successfully initialized!");
    }
    else{
        printModuleName(); Serial.println("Radio manager failed to initialize!");
        moduleInitialized = false;
        return;
    }

    // Set the radio frequency
    if(driver.setFrequency(RF95_FREQ)){
        printModuleName(); Serial.println("Radio frequency successfully set to: " + String(RF95_FREQ));
    }
    else{
        printModuleName(); Serial.println("Failed to set frequency!");
        moduleInitialized = false;
        return;
    }

    // Set radio power level
    printModuleName(); Serial.println("Setting device power level to: " + String(powerLevel));
    driver.setTxPower(powerLevel, false);

    // Set timeout time
    printModuleName(); Serial.println("Timeout time set to: " + String(retryTimeout));
    manager->setTimeout(retryTimeout);

    // Set retry attempts
    printModuleName(); Serial.println("Retry count set to: " + String(retryCount));
    manager->setRetries(retryCount);

    // Set bandwidth
    driver.setSignalBandwidth(125000);
    driver.setSpreadingFactor(10); 
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
        uint8_t len = maxMessageLength;

        // Write all null bytes to the buffer
        char buffer[maxMessageLength];
        memset(buffer, '\0', maxMessageLength);

        printModuleName(); Serial.println("Waiting for packet...");

        // Non-blocking receive if time is set to 0
        if(maxWaitTime == 0){
            recvStatus = manager->recvfromAck((uint8_t*)buffer, &len, &fromAddress);
        }
        else{
            recvStatus = manager->recvfromAckTimeout((uint8_t*)buffer, &len, maxWaitTime, &fromAddress);
        }

        // If a packet was received 
        if(recvStatus){
            printModuleName(); Serial.println("Packet Received!");
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
            printModuleName(); Serial.println("No Packet Received");
        }

        driver.sleep();
        return recvStatus;
    }else{
        printModuleName(); Serial.println("Module not initialized!");
        return false;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receivePartial(uint waitTime){
    if(moduleInitialized){
        bool recvStatus = false;
        uint8_t fromAddress;
        uint8_t len = maxMessageLength;

        // Write all null bytes to the buffer
        char buffer[maxMessageLength];
       
        // Contents array to store each module in
        JsonArray contents = manInst->getDocument().createNestedArray("contents");
        
        // Gets the number of additional packets the hub should expect
        int numPackets = manInst->getDocument()["numPackets"].as<int>();

        // Loop for the given number of packets we are expecting
        for(int i = 0; i < numPackets; i++){
            tempDoc.clear();
            printModuleName(); Serial.println("Waiting for packet " + String(i+1) + " / " + String(numPackets));
            memset(buffer, '\0', maxMessageLength);

            // Non-blocking receive if time is set to 0
            if(waitTime == 0){
                recvStatus = manager->recvfromAck((uint8_t*)buffer, &len, &fromAddress);
            }
            else{
                recvStatus = manager->recvfromAckTimeout((uint8_t*)buffer, &len, waitTime, &fromAddress);
            }

            // If a packet was received 
            if(recvStatus){
                printModuleName(); Serial.println("Fragment received " + String(i+1) + " / " + String(numPackets));
                signalStrength = driver.lastRssi();
                recvStatus = bufferToJson(buffer, tempDoc);

                // Add the current module to the overall contents array
                contents.add(tempDoc["contents"][0].as<JsonObject>());
            }
            else{
                printModuleName(); Serial.println("No Packet Received");
            }
        }

        driver.sleep();
        return recvStatus;
    }else{
        printModuleName(); Serial.println("Module not initialized!");
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
        printModuleName(); Serial.println("Module not initialized!");
        return false;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendFull(const uint8_t destinationAddress){
    char buffer[maxMessageLength];

    // Try to write the JSON to the buffer
    if(!jsonToBuffer(buffer, manInst->getDocument().as<JsonObject>())){
        printModuleName(); Serial.println("Failed to convert JSON to MsgPack");
        return false;
    }

    if(!manager->sendtoWait((uint8_t*)buffer, measureMsgPack(manInst->getDocument()), destinationAddress)){
        printModuleName(); Serial.println("Failed to send packet to specified address! The message may have gotten their but not received and acknowledgement response");
    }else{
        printModuleName(); Serial.println("Successfully transmitted packet!");
    }

    signalStrength = driver.lastRssi();
    driver.sleep();
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendPartial(const uint8_t destinationAddress){
    printModuleName(); Serial.println("Packet was greater than the maximum packet length the packet will be fragmented");

    char buffer[maxMessageLength];  
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
        printModuleName(); Serial.println("Failed to convert JSON to MsgPack");
        return false;
    }

    // Send the packet off
    if(!manager->sendtoWait((uint8_t*)buffer, measureMsgPack(obj), destinationAddress)){
        printModuleName(); Serial.println("Failed to send packet to specified address! The message may have gotten their but not received and acknowledgement response");
    }else{
        printModuleName(); Serial.println("Successfully transmitted packet!");
    }

    // Send all modules by themselves to allow for larger amounts of data to be sent over radio
    sendModules(manInst->getDocument().as<JsonObject>(), destinationAddress);

     // If we have a timestamp we also need to copy this across to the new one
    if(!manInst->getDocument()["timestamp"].isNull()){
        JsonObject objTS = obj.createNestedObject("timestamp");
        objTS["time_utc"] = manInst->getDocument()["timestamp"]["time_utc"].as<String>();
        objTS["time_local"] = manInst->getDocument()["timestamp"]["time_local"].as<String>();
    }

    signalStrength = driver.lastRssi();
    driver.sleep();
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendModules(JsonObject json, const uint8_t destinationAddress){
    char buffer[maxMessageLength];  
    JsonObject obj = tempDoc.to<JsonObject>();
    int numPackets = json["contents"].size();

    // Loop through the number of packets we need to send
    for(int i = 0; i < numPackets; i++){
        obj.clear();
        JsonArray objContents = obj.createNestedArray("contents");

        // Create a data object for each content
        JsonObject objData = objContents[0].createNestedObject("data");
        objContents[0]["module"] = json["contents"][i]["module"];
        
        // Get each piece of data that the module had
        JsonObject old_data = json["contents"][i]["data"];
	    for (JsonPair kv : old_data){
		    objData[kv.key()] = kv.value();
	    }

        // Try to write the JSON to the buffer
        if(!jsonToBuffer(buffer, obj)){
            printModuleName(); Serial.println("Failed to convert JSON to MsgPack");
            return false;
        }

        // Send the packet off
        if(!manager->sendtoWait((uint8_t*)buffer, measureMsgPack(obj), destinationAddress)){
            printModuleName(); Serial.println("Failed to send packet to specified address! The message may have gotten their but not received and acknowledgement response");
        }else{
            printModuleName(); Serial.println("Successfully transmitted packet!");
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