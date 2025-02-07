#include "Loom_LoRa.h"
#include "ArduinoJson.hpp"
#include "FatLib/ArduinoFiles.h"
#include "Logger.h"
#include "Module.h"
#include <cstdint>
#include <cstdio>

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LoRa::Loom_LoRa(
    Manager& manager,
    const uint8_t address, 
    const uint8_t powerLevel,
    const uint8_t retryCount, 
    const uint16_t retryTimeout
) : Module("LoRa"),
        manager(&manager), 
        radioDriver{RFM95_CS, RFM95_INT},
        deviceAddress(address),
        powerLevel(powerLevel),
        sendRetryCount(retryCount),
        receiveRetryCount(retryCount),
        retryTimeout(retryTimeout)
{
    this->radioManager = new RHReliableDatagram(
        radioDriver, this->deviceAddress);
    this->manager->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LoRa::Loom_LoRa(
    Manager& manager,
    const uint8_t powerLevel, 
    const uint8_t retryCount, 
    const uint16_t retryTimeout
) : Loom_LoRa(
    manager, 
    manager.get_instance_num(), 
    powerLevel, 
    retryCount, 
    retryTimeout
) {}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LoRa::~Loom_LoRa() {
    delete radioManager;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::initialize() {
    // Set CS pin as pull up
    pinMode(RFM95_CS, INPUT_PULLUP);
    
    // Reset the radio
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);

    // Initialize the radio manager
    if (radioManager->init()) {
        LOG(F("Radio manager successfully initialized!"));

    } else {
        ERROR(F("Radio manager failed to initialize!"));
        moduleInitialized = false;
        return;
    }

    // Set the radio frequency
    if (radioDriver.setFrequency(RF95_FREQ)) {
        snprintf(logOutput, OUTPUT_SIZE, "Radio frequency successfully set to: %f", RF95_FREQ);
        LOG(logOutput);
    } else {
        ERROR(F("Failed to set frequency!"));
        moduleInitialized = false;
        return;
    }

    // Set radio power level
    snprintf(logOutput, OUTPUT_SIZE, "Setting device power level to: %i", powerLevel);
    LOG(logOutput);
    radioDriver.setTxPower(powerLevel, false);

    // Set timeout time
    snprintf(logOutput, OUTPUT_SIZE, "Timeout time set to: %i,", retryTimeout);
    LOG(logOutput);
    radioManager->setTimeout(retryTimeout);

    // Set retry attempts
    snprintf(logOutput, OUTPUT_SIZE, "Transmit retry count set to: %i", sendRetryCount);
    LOG(logOutput);
    radioManager->setRetries(sendRetryCount);

    // Print the set address of the device
    snprintf(logOutput, OUTPUT_SIZE, "Address set to: %i", radioManager->thisAddress());
    LOG(logOutput);
    
    // https://cdn.sparkfun.com/assets/a/e/7/e/b/RFM95_96_97_98W.pdf, Page 22

    // Set bandwidth
    radioDriver.setSignalBandwidth(125000);

    // Higher spreading factors give us more range
    radioDriver.setSpreadingFactor(7); 

    // Coding rate should be 4/5
    radioDriver.setCodingRate4(5);	
    radioDriver.sleep();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::power_up() {
    if (poweredUp) {
        radioDriver.available();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::power_down() {
    if (poweredUp) {
        radioDriver.sleep();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::package() {
    if (!moduleInitialized) {
        return;
    }

    JsonObject json = manager->get_data_object(getModuleName());
    json["RSSI"] = signalStrength;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receiveFromLoRa(uint8_t *buf, uint8_t buf_size, 
                                       uint timeout, uint8_t *fromAddress) {
    bool status = true;

    memset(buf, 0, buf_size);

    LOG(F("Waiting for message..."));

    if (timeout) {
        status = radioManager->recvfromAckTimeout(buf, &buf_size, timeout, 
                                                  fromAddress);
    } else {
        status = radioManager->recvfromAck(buf, &buf_size, 
                                           fromAddress);
    }

    if (!status) {
        WARNING(F("No message received"));
    }

    radioDriver.sleep();
    return status;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
FragReceiveStatus Loom_LoRa::receiveFrag(uint timeout) {
    if (!moduleInitialized) {
        ERROR(F("LoRa module not initialized!"));
        return FragReceiveStatus::Error;
    } 

    uint8_t buf[MAX_MESSAGE_LENGTH] = {};
    uint8_t fromAddress;

    bool recvStatus = receiveFromLoRa(buf, sizeof(buf), timeout, &fromAddress);
    if (!recvStatus) {
        return FragReceiveStatus::Error;
    }

    snprintf(logOutput, OUTPUT_SIZE, "Received packet from %i", fromAddress);
    LOG(logOutput);

    StaticJsonDocument<300> tempDoc;

    // cast buf to const to avoid mutation
    auto err = deserializeMsgPack(tempDoc, (const char *)buf, sizeof(buf));
    if (err != DeserializationError::Ok) {
        snprintf(logOutput, OUTPUT_SIZE, "Error occurred parsing MsgPack: %s", err.c_str());
        ERROR(logOutput);
        return FragReceiveStatus::Error;
    }

    if (tempDoc.containsKey("batch_size")) {
        return FragReceiveStatus::BatchHeader;
    }

    bool isReady = false;
    if (tempDoc.containsKey("numPackets")) {
        isReady = handleFragHeader(tempDoc, fromAddress);

    } else if (frags.find(fromAddress) != frags.end()) {
        isReady = handleFragBody(tempDoc, fromAddress);
    
    } else if (tempDoc.containsKey("module")) {
        isReady = handleLostFrag(tempDoc, fromAddress);

    } else {
        isReady = handleSingleFrag(tempDoc);
    }

    return isReady ? FragReceiveStatus::Complete : FragReceiveStatus::Incomplete;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleFragHeader(JsonDocument &workingDoc, 
                                        uint8_t fromAddress) {
    int expectedFragCount = workingDoc["numPackets"].as<int>();
    workingDoc.remove("numPackets");

    int packetSpace = 300 * (expectedFragCount + 1);

    if (frags.find(fromAddress) != frags.end()) {
        snprintf(logOutput, OUTPUT_SIZE, "Dropping corrupted packet received from %i", fromAddress);
        WARNING(logOutput);

        frags.erase(fromAddress);
    }

    // this should never fail
    auto inserted = frags.emplace(std::make_pair(
        fromAddress,
        PartialPacket { 
            expectedFragCount, 
            DynamicJsonDocument(packetSpace) }));

    // TODO(rwheary): does this copy data over to the correctly sized buffer??
    inserted.first->second.working = workingDoc;

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleFragBody(JsonDocument &workingDoc, 
                                      uint8_t fromAddress) {
    PartialPacket *partialPacket = &frags.find(fromAddress)->second;

    JsonArray contents = partialPacket->working["contents"].as<JsonArray>();
    contents.add(workingDoc);

    partialPacket->remainingFragments--;

    if (partialPacket->remainingFragments == 0) { 
        manager->getDocument() = std::move(partialPacket->working);
        frags.erase(fromAddress);

        return true;
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleSingleFrag(JsonDocument &workingDoc) {
    // TODO(rwheary): does this copy data over to the correctly sized buffer??
    manager->getDocument() = workingDoc;
    
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleLostFrag(JsonDocument &workingDoc, 
                                      uint8_t fromAddress) {
    snprintf(logOutput, OUTPUT_SIZE, "Dropping fragmented packet body with no header received from %i", fromAddress);
    WARNING(logOutput);

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receive(uint timeout) {
    // TODO(rwheary): i think this like, sucks? But if we assume interleaved
    // fragments we lose nice properties like knowing how many packets to 
    // expect.
    // I think this should work but it needs testing.
    bool documentIsReady = false;

    int retryCount = receiveRetryCount;
    while (retryCount > 0) {
        FragReceiveStatus status = receiveFrag(timeout);

        switch (status) {
        case FragReceiveStatus::Complete:
            return true;
        case FragReceiveStatus::BatchHeader:
            return false; // not expecting to receive a batch
        case FragReceiveStatus::Error:
            retryCount--;
            break;
        }
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool Loom_LoRa::receiveBatch(uint timeout) {
    bool status = false;

    if (!moduleInitialized) {
        ERROR(F("Module not initialized!"));
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::transmitToLoRa(JsonObject json, uint8_t destinationAddress) {
    uint8_t buffer[MAX_MESSAGE_LENGTH] = {};
    bool status = false;

    status = serializeMsgPack(json, buffer, MAX_MESSAGE_LENGTH);
    if (!status) {
        ERROR(F("Failed to convert JSON to MsgPack"));
        return false;
    }

    status = radioManager->sendtoWait(buffer, sizeof(buffer), 
                                      destinationAddress);
    if (!status) {
        ERROR(F("Failed to send packet to specified address!"));
        return false;
    }

    LOG(F("Successfully transmitted packet!"));
    signalStrength = radioDriver.lastRssi();
    radioDriver.sleep();
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendFullPacket(JsonObject json, uint8_t destinationAddress) {
    return transmitToLoRa(json, destinationAddress);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendFragmentedPacket(JsonObject json, 
                                     uint8_t destinationAddress) {
    LOG(F("Packet was greater than the maximum packet length; the packet will be fragmented"));
    bool status = false;

    status = json.containsKey("contents");
    if (!status) {
        ERROR(F("JSON data is malformed and cannot be fragmented"));
        return false;
    }
    int numFrags = json["contents"].size();

    status = sendPacketHeader(json, destinationAddress);
    if (!status) {
        ERROR(F("Unable to transmit initial packet header! Split packets will not be sent"));
        return false;
    }

    for (int i = 0; i < numFrags; i++) {
        snprintf(logOutput, OUTPUT_SIZE, "Sending fragmented packet (%i/%i)...", i+1, numFrags);
        LOG(logOutput);

        JsonObject frag = json["contents"][i].as<JsonObject>();
        status = transmitToLoRa(frag, destinationAddress);
        if (!status) {
            ERROR(F("Failed to transmit fragmented packet!"));
            return false;
        }

        // randomizing the delay helps decrease collisions
        delay(random(400, 1000));
    }

    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendPacketHeader(JsonObject json, 
                                 uint8_t destinationAddress) {
    StaticJsonDocument<MAX_MESSAGE_LENGTH * 2> sendDoc;

    sendDoc["type"] = json["type"].as<const char*>();
    sendDoc["numPackets"] = json["contents"].size();
    
    JsonObject objId = sendDoc.createNestedObject("id");
    objId["name"] = json["id"]["name"].as<const char*>();
    objId["instance"] = json["id"]["instance"].as<int>();

    sendDoc.createNestedArray("contents");

    if (!json["timestamp"].isNull()) {
        JsonObject objTimestamp = sendDoc.createNestedObject("timestamp");
        objTimestamp["time_utc"] = 
            json["timestamp"]["time_utc"].as<const char*>();
        objTimestamp["time_local"] = 
            json["timestamp"]["time_local"].as<const char*>();
    }

    JsonObject sendOut = sendDoc.as<JsonObject>();
    return transmitToLoRa(sendOut, destinationAddress);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::send(const uint8_t destinationAddress) {
    return send(destinationAddress, manager->getDocument().as<JsonObject>());
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::send(const uint8_t destinationAddress, 
                            JsonObject json) {
    if (!moduleInitialized) {
        ERROR(F("Module not initialized!"));
        return false;
    }

    if (measureMsgPack(json) > MAX_MESSAGE_LENGTH) {
        return sendFragmentedPacket(json, destinationAddress);
    } else {
        return sendFullPacket(json, destinationAddress);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool Loom_LoRa::sendBatch(const uint8_t destinationAddress) {
    bool status = false;

    if (!moduleInitialized) {
        ERROR(F("Module not initialized!"));
        return false;
    }

    if (!batchSD) {
        ERROR(F("BatchSD module not set - cannot send batch"));
        return false;
    }

    if (!batchSD->shouldPublish()) {
        LOG(F("BatchSD not ready to publish"));
        return true;
    }

    StaticJsonDocument<100> batchNotify;
    int batchSize = batchSD->getBatchSize();
    batchNotify["batch_size"] = batchSize;

    // Send the notification to the other radio to tell it to prepare to expect
    // a batch of data. The packet is formatted as follows:
    // {
    //     "batch_size": <size>
    // }
    status = send(destinationAddress, batchNotify.as<JsonObject>());
    if (!status) {
        ERROR(F("Could not send initial batch notification!"));
        return false;
    }

    File fileOutput = batchSD->getBatch();

    for (int i = 0; i < batchSize && fileOutput.available(); i++) {
        uint8_t packetBuf[2000];
        // read line from file into packetBuf
        int len = fileOutput.readBytesUntil('\n', packetBuf, 
                                            sizeof(packetBuf));

        if (!len) {
            WARNING(F("BatchSD data missing ending newline"));
            break;
        }

        // remove trailing carriage return if DOS line endings have been used
        if (packetBuf[len - 1] == '\r') {
            packetBuf[len - 1] = 0;
        }

        // deserialze packet into main document
        deserializeJson(manager->getDocument(), (const char *)packetBuf,
                        sizeof(packetBuf));

        status = send(destinationAddress);
        if (status) {
            snprintf(logOutput, OUTPUT_SIZE, "Successfully transmitted packet (%i/%i)", i+1, batchSize);
            LOG(logOutput);
        } else {
            snprintf(logOutput, OUTPUT_SIZE, "Failed to transmit packet (%i/%i)", i+1, batchSize);
            ERROR(logOutput);
            // TODO(rwheary): should this fail the entire send?
        }

        delay(500);
        // TODO(rwheary): why?
        Serial.println();
    }

    fileOutput.close();
}
