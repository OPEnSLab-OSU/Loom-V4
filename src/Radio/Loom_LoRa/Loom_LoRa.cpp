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
    const uint8_t sendMaxRetries,
    const uint8_t receiveMaxRetries,
    const uint16_t retryTimeout
) : Module("LoRa"),
        manager(&manager), 
        radioDriver{RFM95_CS, RFM95_INT},
        deviceAddress(address),
        powerLevel(powerLevel),
        sendRetryCount(sendMaxRetries),
        receiveRetryCount(receiveMaxRetries),
        retryTimeout(retryTimeout),
        expectedOutstandingPackets(0)
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
        LOGF("Radio frequency successfully set to: %f", RF95_FREQ);
    } else {
        ERROR(F("Failed to set frequency!"));
        moduleInitialized = false;
        return;
    }

    // Set radio power level
    LOGF("Setting device power level to: %i", powerLevel);
    radioDriver.setTxPower(powerLevel, false);

    // Set timeout time
    LOGF("Timeout time set to: %i,", retryTimeout);
    radioManager->setTimeout(retryTimeout);

    // Set retry attempts
    LOGF("Transmit retry count set to: %i", sendRetryCount);
    radioManager->setRetries(sendRetryCount);

    // Print the set address of the device
    LOGF("Address set to: %i", radioManager->thisAddress());
    
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
    if (batchSD) {
        int currentBatch = batchSD->getCurrentBatch();
        int batchSize = batchSD->getBatchSize();
        poweredUp = currentBatch == batchSize - 1;
    }

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
void Loom_LoRa::setAddress(const uint8_t newAddress) {
    deviceAddress = newAddress;
    radioManager->setThisAddress(newAddress);
    radioDriver.sleep();
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
FragReceiveStatus Loom_LoRa::receiveFrag(uint timeout, bool shouldProxy,
                                         uint8_t* fromAddress) {
    if (!moduleInitialized) {
        ERROR(F("LoRa module not initialized!"));
        return FragReceiveStatus::Error;
    }

    clearExpiredHandshake();

    uint8_t buf[MAX_MESSAGE_LENGTH] = {};

    bool recvStatus = receiveFromLoRa(buf, sizeof(buf), timeout, fromAddress);
    if (!recvStatus) {
        clearExpiredHandshake();
        return FragReceiveStatus::Error;
    }

    LOGF("Received packet from %i", *fromAddress);

    StaticJsonDocument<300> tempDoc;

    // cast buf to const to avoid mutation
    auto err = deserializeMsgPack(tempDoc, (const char *)buf, sizeof(buf));
    if (err != DeserializationError::Ok) {
        ERRORF("Error occurred parsing MsgPack: %s", err.c_str());
        return FragReceiveStatus::Error;
    }

    // Always check for handshake packets first
    if (tempDoc.containsKey("handshake")) {
        const char *handshakeValue = tempDoc["handshake"].as<const char*>();
        if (!handshakeValue) {
            ERROR(F("[HANDSHAKE] Invalid handshake packet"));
            return FragReceiveStatus::Error;
        }

        if (strcmp(handshakeValue, "Request") == 0
            && this->handshakeEstablished
            && !clearExpiredHandshake()) {
            LOGF("[HANDSHAKE] Hub already waiting for payload from %i",
                 this->handshakePeerAddress);
            return FragReceiveStatus::Incomplete;
        }

        HandshakeReceiveStatus handshakeStatus = handleHandshakeReceive(tempDoc, fromAddress);
        if (handshakeStatus == HandshakeReceiveStatus::Accepted) {
            return FragReceiveStatus::HandshakeAccepted;
        }

        if (handshakeStatus == HandshakeReceiveStatus::AwaitingPayload) {
            return FragReceiveStatus::Incomplete;
        }

        return FragReceiveStatus::Error;
    }

    // If a device reaches a point where its receiving a packet that is NOT a
    // handshake packet:
    // - That means that a handshake was succesfully established 
    // - So, once normal receive is done we can end the handshake
    
    // Clear an active handshake only when the payload comes from the node that
    // was accepted, so another node cannot accidentally reset the handshake.
    if (this->handshakeEstablished) {
        if (*fromAddress == this->handshakePeerAddress) {
            clearHandshake();

        } else if (!clearExpiredHandshake()) {
            LOGF("[HANDSHAKE] Ignoring packet from %i while waiting for %i",
                 *fromAddress, this->handshakePeerAddress);
            return FragReceiveStatus::Incomplete;
        }
    }

    bool isReady = false;
    if (tempDoc.containsKey("batch_size")) {
        isReady = handleBatchHeader(tempDoc);

    } else if (tempDoc.containsKey("numPackets")) {
        isReady = handleFragHeader(tempDoc, *fromAddress);

    } else if (frags.find(*fromAddress) != frags.end()) {
        isReady = handleFragBody(tempDoc, *fromAddress);
    
    } else if (tempDoc.containsKey("module")) {
        isReady = handleLostFrag(tempDoc, *fromAddress);

    } else {
        isReady = handleSingleFrag(tempDoc);
    }

    if (isReady) {
        if (shouldProxy) {
            const char *name = manager->getDocument()["id"]["name"];
            manager->set_device_name(name);

            int instNum = manager->getDocument()["id"]["instance"];
            manager->set_instance_num(instNum);
        }

        if (expectedOutstandingPackets > 0) {
            expectedOutstandingPackets--;
        }

        return FragReceiveStatus::Complete;
    } else {
        return FragReceiveStatus::Incomplete;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
HandshakeReceiveStatus Loom_LoRa::handleHandshakeReceive(JsonDocument &tempDoc, uint8_t* fromAddress) {
    const char *handshakeValue = tempDoc["handshake"].as<const char*>();
    if (!handshakeValue) {
        return HandshakeReceiveStatus::Failed;
    }

    if (strcmp(handshakeValue, "Accept") == 0) {
        return HandshakeReceiveStatus::Accepted;
    } else if (strcmp(handshakeValue, "Request") == 0) {
        LOGF("[HANDSHAKE] Hub sending handshake response");
        if (sendHandshakeResponse(*fromAddress)) {
            beginHandshake(*fromAddress);
            return HandshakeReceiveStatus::AwaitingPayload;
        }
    }

    return HandshakeReceiveStatus::Failed;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::beginHandshake(uint8_t peerAddress) {
    this->handshakeEstablished = true;
    this->handshakePeerAddress = peerAddress;
    this->handshakeEstablishedAt = millis();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::clearHandshake() {
    this->handshakeEstablished = false;
    this->handshakePeerAddress = 0;
    this->handshakeEstablishedAt = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::clearExpiredHandshake() {
    if (!this->handshakeEstablished) {
        return false;
    }

    if (millis() - this->handshakeEstablishedAt < HANDSHAKE_TIMEOUT_MS) {
        return false;
    }

    LOGF("[HANDSHAKE] Timed out waiting for payload; resetting handshake");
    clearHandshake();
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleBatchHeader(JsonDocument &tempDoc) {
    int batch_size = tempDoc["batch_size"];
    LOGF("Received batch header, expecting %i packets", batch_size);
    expectedOutstandingPackets += batch_size;
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleFragHeader(JsonDocument &workingDoc, 
                                        uint8_t fromAddress) {
    int expectedFragCount = workingDoc["numPackets"].as<int>();
    workingDoc.remove("numPackets");

    int packetSpace = 300 * (expectedFragCount + 1);

    if (frags.find(fromAddress) != frags.end()) {
        WARNINGF("Dropping corrupted packet received from %i", fromAddress);

        frags.erase(fromAddress);
    }

    // this should never fail
    auto inserted = frags.emplace(std::make_pair(
        fromAddress,
        PartialPacket { 
            expectedFragCount, 
            DynamicJsonDocument(packetSpace) }));

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
        // overwrite the manager document by deep-copying the finalized packet
        manager->getDocument().set(partialPacket->working);
        frags.erase(fromAddress);

        return true;
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleSingleFrag(JsonDocument &workingDoc) {
    // overwrite the manager document by deep-copying the finalized packet
    manager->getDocument().set(workingDoc);
    
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleLostFrag(JsonDocument &workingDoc, 
                                      uint8_t fromAddress) {
    WARNINGF("Dropping fragmented packet body with no header received from %i",
             fromAddress);

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receive(uint timeout, uint8_t* fromAddress, bool shouldProxy) {
    int retryCount = receiveRetryCount;
    while (retryCount > 0) {
        FragReceiveStatus status = receiveFrag(timeout, shouldProxy, fromAddress);

        switch (status) {
        case FragReceiveStatus::Complete:
            return true;
        case FragReceiveStatus::HandshakeAccepted:
        case FragReceiveStatus::Incomplete:
            break;
        case FragReceiveStatus::Error:
            retryCount--;
            break;
        }
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool Loom_LoRa::receive(uint timeout, bool shouldProxy) {
    uint8_t fromAddress;
    return receive(timeout, &fromAddress, shouldProxy);
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
        LOGF("Sending fragmented packet (%i/%i)...", i+1, numFrags);

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
    // Before devices send normal packets, a handshake packet will be sent
    uint8_t handshakeRetries = 2;
    while(handshakeRetries > 0) {
        bool handshakeTransmitSuccess = false;
        bool handshakeAccepted = false;
        // Send handshake request to hub
        handshakeTransmitSuccess = sendHandshakeRequest(destinationAddress);
        if(handshakeTransmitSuccess) {
            LOGF("[HANDSHAKE] Handshake request sent succesfully");
            // Wait for accept response from hub
            handshakeAccepted = handshakeReceive(destinationAddress);
        }

        if(handshakeAccepted) {
            return send(destinationAddress, manager->getDocument().as<JsonObject>());
        }

        else {
            handshakeRetries--;
            LOGF("[HANDSHAKE] Pausing to retry, %lu attempts remaining", handshakeRetries);
            const uint32_t currentTime = millis();
            while((currentTime + 10000) > millis()) {

            }
        }
    }
    // Once out of retries, fail
    LOGF("Send failed");
    return false;
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

//////////////////////////////////////////////////////////////////////////////////////////////////////
// HANDSHAKE TO DO
// PROGRAMMING SIDE:
// Simplify code comments
// Go over proper practices with Loom team

// HARDWARE/TESTING SIDE:
// Test more robust error modes
// Possibly implement sleep in .ino file instead of waiting in send function

bool Loom_LoRa::sendHandshakeRequest(const uint8_t destinationAddress) {
    const uint8_t HANDSHAKE_SIZE = 100; // enough for the handshake key and string value
    StaticJsonDocument<HANDSHAKE_SIZE> handshakeDoc;
    // Special handshake packet with "Request" value
    handshakeDoc["handshake"] = "Request";

    // Send special handshake packet and check if it went through
    bool handshakeTransmitStatus = sendFullPacket(handshakeDoc.as<JsonObject>(), destinationAddress);
    if(!handshakeTransmitStatus) {
        LOGF("[HANDSHAKE] Sending handshake request failed, hub busy");
        return false;
    }
    return true;
}

bool Loom_LoRa::sendHandshakeResponse(const uint8_t destinationAddress) {
    const uint8_t HANDSHAKE_SIZE = 100; // enough for the handshake key and string value
    StaticJsonDocument<HANDSHAKE_SIZE> handshakeDoc;
    // Special handshake packet with "Accept" value
    handshakeDoc["handshake"] = "Accept";

    bool handshakeTransmitStatus = sendFullPacket(handshakeDoc.as<JsonObject>(), destinationAddress);
    if(!handshakeTransmitStatus) {
        LOGF("[HANDSHAKE] Sending handshake acceptance failed");
        return false;
    }
    LOGF("[HANDSHAKE] Hub succesfully sent handshake acceptance");
    return true;
}

bool Loom_LoRa::handshakeReceive(const uint8_t destinationAddress) {
    // Function for node waiting to receive acceptance packet from hub
    LOGF("[HANDSHAKE] Attempting to receive acceptance from hub");

    int retryCount = receiveRetryCount;
    while (retryCount > 0) {
        uint8_t buf[MAX_MESSAGE_LENGTH] = {};
        uint8_t fromAddress = 0;

        if (!receiveFromLoRa(buf, sizeof(buf), 5000, &fromAddress)) {
            retryCount--;
            continue;
        }

        StaticJsonDocument<300> tempDoc;
        auto err = deserializeMsgPack(tempDoc, (const char *)buf, sizeof(buf));
        if (err != DeserializationError::Ok) {
            ERRORF("[HANDSHAKE] Error parsing acceptance packet: %s", err.c_str());
            retryCount--;
            continue;
        }

        const char *handshakeValue = tempDoc["handshake"].as<const char*>();
        if (!handshakeValue || strcmp(handshakeValue, "Accept") != 0) {
            WARNING(F("[HANDSHAKE] Ignoring non-acceptance packet"));
            retryCount--;
            continue;
        }

        if (fromAddress != destinationAddress) {
            WARNINGF("[HANDSHAKE] Ignoring acceptance from unexpected address %i",
                     fromAddress);
            retryCount--;
            continue;
        }

        LOGF("[HANDSHAKE] Handshake accepted");
        return true;
    }

    LOGF("[HANDSHAKE] Handshake rejected");
    return false;
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

    File fileOutput = batchSD->getBatch();
    int batchSize = batchSD->getBatchSize();

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
            LOGF("Successfully transmitted packet (%i/%i)", i+1, batchSize);
        } else {
            ERRORF("Failed to transmit packet (%i/%i)", i+1, batchSize);
        }

        delay(500);

        Serial.println();
    }

    fileOutput.close();
    return status;
}

bool Loom_LoRa::receiveBatch(uint timeout, int* numberOfPackets) {
    uint8_t fromAddress;
    return receiveBatch(timeout, numberOfPackets, &fromAddress);
}

bool Loom_LoRa::receiveBatch(uint timeout, int* numberOfPackets, uint8_t *fromAddress) {
    bool status = receive(timeout, fromAddress, true);
    *numberOfPackets = expectedOutstandingPackets;
    return status;
}

