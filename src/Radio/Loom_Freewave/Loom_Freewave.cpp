#include "Loom_Freewave.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Freewave::Loom_Freewave(
        Manager& man,
        const uint8_t address, 
        const uint16_t max_message_len, 
        const uint8_t retryCount, 
        const uint16_t retryTimeout
    ) : Radio("Freewave"), manInst(&man), serial1(Serial1), driver(serial1), manager(driver, address)
    {
        this->deviceAddress = address;
        this->retryCount = retryCount;
        this->retryTimeout = retryTimeout;
        this->maxMessageLength = max_message_len;
        manInst->registerModule(this);
    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Freewave::initialize(){

    // Start serial communication with radio
    serial1.begin(115200);

    // Set timeout time
    printModuleName(); Serial.println("Timeout time set to: " + String(retryTimeout));
    manager.setTimeout(retryTimeout);

    // Set retry attempts
    printModuleName(); Serial.println("Retry count set to: " + String(retryCount));
    manager.setRetries(retryCount);

    // Initialize the radio manager
    if(manager.init()){
        printModuleName(); Serial.println("Radio manager successfully initialized!");
    }
    else{
        printModuleName(); Serial.println("Radio manager failed to initialize!");
        moduleInitialized = false;
        return;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Freewave::setAddress(uint8_t addr){
    deviceAddress = addr;
    manager.setThisAddress(addr);
    driver.sleep();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Freewave::receive(uint maxWaitTime){
    bool recvStatus = false;
    uint8_t fromAddress;
    uint8_t len = maxMessageLength;

    // Write all null bytes to the buffer
    char buffer[maxMessageLength];
    memset(buffer, '\0', maxMessageLength);

    printModuleName(); Serial.println("Waiting for packet...");

    // Non-blocking receive if time is set to 0
    if(maxWaitTime == 0){
        recvStatus = manager.recvfromAck((uint8_t*)buffer, &len, &fromAddress);
    }
    else{
        recvStatus = manager.recvfromAckTimeout((uint8_t*)buffer, &len, maxWaitTime, &fromAddress);
    }

    // If a packet was received 
    if(recvStatus){
        printModuleName(); Serial.println("Packet Received!");
        signalStrength = driver.lastRssi();
        recvStatus = bufferToJson(buffer, manInst->getDocument().as<JsonObject>());
    }
    else{
        printModuleName(); Serial.println("No Packet Received");
    }

    driver.sleep();
    return recvStatus;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Freewave::send(const uint8_t destinationAddress){
    char buffer[maxMessageLength];

    // Try to write the JSON to the buffer
    if(!jsonToBuffer(buffer, manInst->getDocument().as<JsonObject>())){
        printModuleName(); Serial.println("Failed to convert JSON to MsgPack");
        return false;
    }

    if(!manager.sendtoWait((uint8_t*)buffer, measureMsgPack(manInst->getDocument()), destinationAddress)){
        printModuleName(); Serial.println("Failed to send packet to specified address!");
        return false;
    }

    printModuleName(); Serial.println("Succsessfully transmit packet!");
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