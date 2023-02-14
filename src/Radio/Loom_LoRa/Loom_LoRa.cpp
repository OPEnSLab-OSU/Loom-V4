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
bool Loom_LoRa::receive(uint maxWaitTime){
    if(moduleInitialized){

        // Wait for packet to arrive
        if(recv(maxWaitTime)){
            manInst->set_device_name(recvDoc["id"]["name"].as<String>());
            manInst->set_instance_num(recvDoc["id"]["instance"].as<int>());

            // Set the main document to the contents of the received
            manInst->getDocument().set(recvDoc);
        
            // Check if we have a numPackets field which tells us we should expect more packets
            if(!manInst->getDocument()["numPackets"].isNull()){
                return receivePartial(maxWaitTime);
            }
            return true;
        }
        else{
            return false;
        }

    }else{
        printModuleName("Module not initialized!");
        return false;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receivePartial(uint waitTime){
    if(moduleInitialized){
        // Gets the number of additional packets the hub should expect
        int numPackets = manInst->getDocument()["numPackets"].as<int>();
        JsonArray contents = manInst->getDocument()["contents"].as<JsonArray>();

        // Loop for the given number of packets we are expecting
        for(int i = 0; i < numPackets; i++){
            printModuleName("Waiting for packet " + String(i+1) + " / " + String(numPackets));

            // If a packet was received 
            if(recv(waitTime)){
                printModuleName("Fragment received " + String(i+1) + " / " + String(numPackets));

                // Add the current module to the overall contents array
                contents.add(recvDoc.as<JsonObject>());
                return true;
            }
            else{
                printModuleName("No Packet Received");
                return false;
            }
        }

        return false;
    }else{
        printModuleName("Module not initialized!");
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
    printModuleName("Packet was greater than the maximum packet length the packet will be fragmented");
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
    sendDoc["type"] = manInst->getDocument()["type"].as<String>();

    // Re-construct the packet header including the number of packets to expect after this initial one
    JsonObject objID = sendDoc.createNestedObject("id");
    objID["name"] = manInst->getDocument()["id"]["name"].as<String>();   
    objID["instance"] = manInst->getDocument()["id"]["instance"].as<int>();

    // Gets the number of additional packets the hub should expect
    int numPackets = manInst->getDocument()["contents"].size();
    obj["numPackets"] = numPackets;

    // Create an empty contents array to preserve formatting
    sendDoc.createNestedArray("contents");

    // If we have a timestamp we also need to copy this across to the new one, MUST BE BEFORE
    if(!manInst->getDocument()["timestamp"].isNull()){
        JsonObject objTS = obj.createNestedObject("timestamp");
        objTS["time_utc"] = manInst->getDocument()["timestamp"]["time_utc"].as<String>();
        objTS["time_local"] = manInst->getDocument()["timestamp"]["time_local"].as<String>();
    }
   
    if(!transmit(sendDoc.as<JsonObject>(), destinationAddress)){
        printModuleName("Unable to transmit initial packet fragmentation notice! Split packets will not be sent");
        return false;
    }

    // Send all modules by themselves to allow for larger amounts of data to be sent over radio
    return sendModules(manInst->getDocument().as<JsonObject>(), numPackets, destinationAddress);;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendModules(JsonObject json, int numModules, const uint8_t destinationAddress){

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
        Watchdog.reset();

        // Set the module key to whatever the main one is
        JsonArray contents = manInst->getDocument()["contents"].as<JsonArray>();
        sendDoc.set(contents[i].as<JsonObject>());

        printModuleName("Fragmented Packet Being Sent...")
        serializeJsonPretty(sendDoc, Serial);

        // Attempt to transmit the document to the other device
        if(!transmit(sendDoc.as<JsonObject>(), destinationAddress)){
            printModuleName("Failed to transmit fragmented packet!");
        }
        delay(3000);
        Watchdog.reset();
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
bool Loom_LoRa::transmit(JsonObject& json, int destination){
    // Buffer of the data being sent
    uint8_t buffer[maxMessageLength];

    // Try to write the JSON to the buffer
    if(!jsonToBuffer(buffer, json)){
        printModuleName("Failed to convert JSON to MsgPack");
        return false;
    }

    Watchdog.disable();
    if(!manager->sendtoWait(buffer, sizeof(buffer), destination)){
        printModuleName("Failed to send packet to specified address! The message may have gotten there but not received and acknowledgement response");
    }else{
        printModuleName("Successfully transmitted packet!");
    }
    Watchdog.enable(WATCHDOG_TIMEOUT);

    signalStrength = driver.lastRssi();
    driver.sleep();
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::recv(int waitTime){
    bool recvStatus = false;
    uint8_t fromAddress;
    
    // Write all null bytes to the buffer
    uint8_t buffer[maxMessageLength];
    uint8_t len = sizeof(buffer);

    printModuleName("Waiting for packet...");

    // Non-blocking receive if time is set to 0
    if(waitTime == 0){
        recvStatus = manager->recvfromAck(buffer, &len, &fromAddress);
    }
    else{
        Watchdog.disable();
        recvStatus = manager->recvfromAckTimeout(buffer, &len, waitTime, &fromAddress);
        Watchdog.enable(WATCHDOG_TIMEOUT);
        
    }
    // If a packet was received 
    if(recvStatus){
        printModuleName("Packet Received!");
        signalStrength = driver.lastRssi();
        recvStatus = bufferToJson(buffer);
    }
    else{
        printModuleName("No Packet Received");
    }
    driver.sleep();
    return recvStatus;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
