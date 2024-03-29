#include "Loom_Freewave.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Freewave::Loom_Freewave(
        Manager& man,
        const uint8_t address, 
        const uint16_t max_message_len, 
        const uint8_t retryCount, 
        const uint16_t retryTimeout
    ) : Radio("Freewave"), manInst(&man), serial1(Serial1), driver(serial1)
    {
        if(address == -1)
            this->deviceAddress = manInst->get_instance_num();
        else
            this->deviceAddress = address;

        manager = new RHReliableDatagram(driver, this->deviceAddress);
        this->retryCount = retryCount;
        this->retryTimeout = retryTimeout;
        this->maxMessageLength = max_message_len;
        manInst->registerModule(this);
    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Freewave::initialize(){
    char output[OUTPUT_SIZE];
    // Start serial communication with radio
    serial1.begin(115200);

    // Set timeout time
    snprintf(output, OUTPUT_SIZE, "Timeout time set to: %u", retryTimeout);
    LOG(output);
    manager->setTimeout(retryTimeout);

    // Set retry attempts
    snprintf(output, OUTPUT_SIZE, "Retry count set to: %u", retryCount);
    LOG(output);
    manager->setRetries(retryCount);

    // Initialize the radio manager
    if(manager->init()){
        LOG(F("Radio manager successfully initialized!"));
    }
    else{
        ERROR(F("Radio manager failed to initialize!"));
        moduleInitialized = false;
        return;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Freewave::package(){
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["RSSI"] = getSignalStrength();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Freewave::setAddress(uint8_t addr){
    deviceAddress = addr;
    manager->setThisAddress(addr);
    driver.sleep();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Freewave::receive(uint maxWaitTime){
    bool recvStatus = false;
    uint8_t fromAddress;
    

    // Write all null bytes to the buffer
    uint8_t buffer[maxMessageLength];
    uint8_t len = sizeof(buffer);

    LOG(F("Waiting for packet..."));

    // Non-blocking receive if time is set to 0
    if(maxWaitTime == 0){
        recvStatus = manager->recvfromAck(buffer, &len, &fromAddress);
    }
    else{
        recvStatus = manager->recvfromAckTimeout(buffer, &len, maxWaitTime, &fromAddress);
    }

    // If a packet was received 
    if(recvStatus){
        LOG(F("Packet Received!"));
        signalStrength = driver.lastRssi();
        recvStatus = bufferToJson(buffer);
        size_t jsonSize = measureJson(recvDoc)+1;
        recvData = (char *) malloc(jsonSize);
        serializeJson(recvDoc, recvData, jsonSize);
        deserializeJson(manInst->getDocument(), recvData);
        free(recvData);

        // Update device name
        manInst->set_device_name(manInst->getDocument()["id"]["name"].as<const char*>());
        manInst->set_instance_num(manInst->getDocument()["id"]["instance"].as<int>());
        
    }
    else{
        WARNING(F("No Packet Received"));
    }

    driver.sleep();
    return recvStatus;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Freewave::send(const uint8_t destinationAddress){
    uint8_t buffer[maxMessageLength];

    // Try to write the JSON to the buffer
    if(!jsonToBuffer(buffer, manInst->getDocument().as<JsonObject>())){
        ERROR(F("Failed to convert JSON to MsgPack"));
        return false;
    }

    if(!manager->sendtoWait((uint8_t*)buffer, sizeof(buffer), destinationAddress)){
        ERROR(F("Failed to send packet to specified address!"));
        return false;
    }

    LOG(F("Successfully transmit packet!"));
    signalStrength = driver.lastRssi();
    driver.sleep();
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Freewave::power_up(){
    driver.available();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Freewave::power_down(){
    driver.sleep();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////