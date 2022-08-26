#include "Loom_LoRa.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LoRa::Loom_LoRa(
        Manager& man,
        const uint8_t address, 
        const uint8_t powerLevel, 
        const uint8_t retryCount, 
        const uint16_t retryTimeout,
        const uint16_t max_message_len
    ) : Radio("LoRa"), manInst(&man), driver{RFM95_CS, RFM95_INT}, manager {driver, address}
    {
        this->deviceAddress = address;
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
    if(manager.init()){
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
    manager.setTimeout(retryTimeout);

    // Set retry attempts
    printModuleName(); Serial.println("Retry count set to: " + String(retryCount));
    manager.setRetries(retryCount);

    // Set bandwidth
    driver.setSignalBandwidth(125000);
    driver.setSpreadingFactor(10); 
	driver.setCodingRate4(8);	
	driver.sleep();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::setAddress(uint8_t addr){
    deviceAddress = addr;
    manager.setThisAddress(addr);
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
            recvStatus = manager.recvfromAck((uint8_t*)buffer, &len, &fromAddress);
        }
        else{
            recvStatus = manager.recvfromAckTimeout((uint8_t*)buffer, &len, maxWaitTime, &fromAddress);
        }

        // If a packet was received 
        if(recvStatus){
            printModuleName(); Serial.println("Packet Received!");
            signalStrength = driver.lastRssi();
            recvStatus = bufferToJson(buffer, manInst->getDocument().to<JsonObject>());
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
bool Loom_LoRa::send(const uint8_t destinationAddress){
    if(moduleInitialized){
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

        printModuleName(); Serial.println("Successfully transmitted packet!");
        signalStrength = driver.lastRssi();
        driver.sleep();
        return true;
    }
    else{
        printModuleName(); Serial.println("Module not initialized!");
        return false;
    }
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