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
    const uint8_t handshakeMaxRetries,
    const uint8_t sendMaxRetries,
    const uint8_t receiveMaxRetries,
    const uint16_t retryTimeout
) : Module("LoRa"),
        manager(&manager), 
        radioDriver{RFM95_CS, RFM95_INT},
        deviceAddress(address),
        powerLevel(powerLevel),
        handshakeRetryCount(handshakeMaxRetries),
        sendRetryCount(sendMaxRetries),
        receiveRetryCount(receiveMaxRetries),
        retryTimeout(retryTimeout),
        expectedOutstandingPackets(0),
        lastArrivalTime(0)
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

    // radioManager methods modify buf, buf_size, and fromAddress
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
bool Loom_LoRa::handleHandshakeRequest(const JsonObject& tempDoc, uint8_t fromAddress) {
    bool acceptHandshake = false;
    if (tempDoc.containsKey("handshake") && 
        strcmp(tempDoc["handshake"], "Request") == 0) {

        // Build response packet (Accept or Deny)
        const uint8_t HANDSHAKE_SIZE = 300;
        StaticJsonDocument<HANDSHAKE_SIZE> handshakeDoc;

        // DECISION: Accept or Deny based on current handshake state
        if (!this->handshakeEstablished) {
            // CASE 1: Not currently in handshake - ACCEPT new request
            handshakeDoc["handshake"] = "Accept";
            this->activePartner = fromAddress;
            this->handshakeEstablished = true;
            this->expectedOutstandingPackets = 0;
            acceptHandshake = true;

            LOGF("ACCEPT REQUEST I");
        } else {
            // CASE 2: Currently in handshake with another device
            unsigned long timeSinceLastPacket = millis() - this->lastArrivalTime;
            
            if (timeSinceLastPacket > 10000) {
                // CASE 2a: Old handshake timed out (>10s) - ACCEPT new request
                frags.erase(activePartner); // Clean up old fragments

                handshakeDoc["handshake"] = "Accept";
                this->activePartner = fromAddress;
                this->handshakeEstablished = true;
                this->expectedOutstandingPackets = 0;
                acceptHandshake = true;
                LOGF("ACCEPT REQUEST II");
            }
            else {
                // CASE 2b: Still in active handshake (<10s) - DENY request
                handshakeDoc["handshake"] = "Deny";
                acceptHandshake = false;
                LOGF("DENY REQUEST");
            }
        }
    
        // SEND response packet
        const char* responseType = strcmp(handshakeDoc["handshake"], "Accept") == 0 ? "ACCEPT" : "DENY";
        // LOGF("[HANDSHAKE] Sending %s response to device %i", responseType, fromAddress);
        bool handshakeTransmitStatus = sendFullPacket(handshakeDoc.as<JsonObject>(), fromAddress);
        
        if (handshakeTransmitStatus) {
            // LOGF("[HANDSHAKE] %s response transmitted to %i ✓", responseType, fromAddress);
        } else {
            if (acceptHandshake) {
                // If we accepted but failed to send, reset state to recover
                this->handshakeEstablished = false;
                this->activePartner = -1;
            }
            acceptHandshake = false;
        }
    }
    return acceptHandshake;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
FragReceiveStatus Loom_LoRa::receiveFrag(uint timeout, bool shouldProxy,
                                         uint8_t* fromAddress) {
    if (!moduleInitialized) {
        return FragReceiveStatus::Error;
    }

    uint8_t buf[MAX_MESSAGE_LENGTH] = {};

    bool recvStatus = receiveFromLoRa(buf, sizeof(buf), timeout, fromAddress);
    if (!recvStatus) {
        return FragReceiveStatus::Error;
    }

    StaticJsonDocument<300> tempDoc;

    // cast buf to const to avoid mutation
    auto err = deserializeMsgPack(tempDoc, (const char *)buf, sizeof(buf)); 
    if (err != DeserializationError::Ok) {
        return FragReceiveStatus::Error;
    }

    if (tempDoc.containsKey("handshake")) {
        bool acceptHandshake = false;
        acceptHandshake = handleHandshakeRequest(tempDoc.as<JsonObject>(), *fromAddress); 
        if(acceptHandshake) {
            return FragReceiveStatus::Complete;
        }
        if(!acceptHandshake) {
            return FragReceiveStatus::Error;
        }
    }

    // Handshake WALL - process won't parse any fragments that aren't from handshake relationship
    if(!this->handshakeEstablished || *fromAddress != this->activePartner) {
        return FragReceiveStatus::Error;
    }
    
    // adjust last frag arrival time of an acceptable packet
    this->lastArrivalTime = millis(); 

    bool isReady = false;
    if (tempDoc.containsKey("batch_size")) {
        isReady = handleBatchHeader(tempDoc);

    // NOTE: numPackets is referring to the number of fragments, not actual packets.
    } else if (tempDoc.containsKey("numPackets")) {
        isReady = handleFragHeader(tempDoc, *fromAddress);
    } else if (this->handshakeEstablished && frags.find(*fromAddress) != frags.end()) {
        isReady = handleFragBody(tempDoc, *fromAddress);
    } else if (tempDoc.containsKey("module")) {
        isReady = handleLostFrag(tempDoc, *fromAddress);
    } else {
        isReady = handleSingleFrag(tempDoc);
    }

    if (isReady) {
        if (shouldProxy) {
            // Set this current device's name and instance number to match the sender of the received packets
            const char *name = manager->getDocument()["id"]["name"];
            manager->set_device_name(name);

            int instNum = manager->getDocument()["id"]["instance"];
            manager->set_instance_num(instNum);
        }

        return FragReceiveStatus::Complete;
    } else {
        return FragReceiveStatus::Incomplete;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleBatchHeader(JsonDocument &tempDoc) {
    int batch_size = tempDoc["batch_size"];
    LOGF("Received batch header, expecting %i completed packets", batch_size);
    expectedOutstandingPackets += batch_size;
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleFragHeader(JsonDocument &workingDoc, 
                                        uint8_t fromAddress) {
    // NOTE: numPackets is referring to the number of fragments, not actual packets.
    int expectedFragCount = workingDoc["numPackets"].as<int>();
    workingDoc.remove("numPackets");

    int packetSpace = 300 * (expectedFragCount + 1);

    // check frags map for existing entry from the same address. Assumes new header is more valid.
    if (frags.find(fromAddress) != frags.end()) {
        WARNINGF("Dropping old packet data received from %i", fromAddress);

        frags.erase(fromAddress);   // New frag header => delete existing partial packet in frags
    }

    // This will always just append the new fragment header into the array regardless of any other circumstances
    //      this should never fail
    auto inserted = frags.emplace(std::make_pair(
        fromAddress,
        PartialPacket { 
            expectedFragCount, 
            DynamicJsonDocument(packetSpace) }));

    // inserts the fragment header into the working json doc in the frags map
    inserted.first->second.working = workingDoc;

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleFragBody(JsonDocument &workingDoc, 
                                      uint8_t fromAddress) {
    PartialPacket *partialPacket = &frags.find(fromAddress)->second;

    // contents is a reference handle into working document
    JsonArray contents = partialPacket->working["contents"].as<JsonArray>();
    contents.add(workingDoc);

    partialPacket->remainingFragments--;

    if (partialPacket->remainingFragments == 0) { 
        // overwrite the manager document by deep-copying the finalized packet
        manager->getDocument().set(partialPacket->working);
        frags.erase(fromAddress);

        if (this->expectedOutstandingPackets > 0) {
            this->expectedOutstandingPackets--;
            // LOGF("[FRAGBODY] Decremented expectedOutstandingPackets to %i", this->expectedOutstandingPackets);
        }

        if(this->expectedOutstandingPackets == 0) {
            // LOG("[FRAGBODY] Received final packet - all batch packets now complete. Dropping handshake.");
            this->handshakeEstablished = false; 
            this->activePartner = -1; 
        } else {
            // LOGF("[FRAGBODY] More packets expected. Remaining: %i", this->expectedOutstandingPackets);
        }

        return true;
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleSingleFrag(JsonDocument &workingDoc) {
    // overwrite the manager document by deep-copying the finalized packet
    manager->getDocument().set(workingDoc);
    
    if (this->expectedOutstandingPackets > 0) {
        this->expectedOutstandingPackets--;
        // LOGF("[SINGLEFRAG] Decremented expectedOutstandingPackets to %i", this->expectedOutstandingPackets);
    }
    
    if(this->expectedOutstandingPackets == 0) {
        // LOG("[SINGLEFRAG] Received full single packet - all batch packets complete. Dropping handshake!");
        this->handshakeEstablished = false;
        this->activePartner = -1;
    } else {
        // LOGF("[SINGLEFRAG] Received complete packet but expecting more. Remaining: %i", this->expectedOutstandingPackets);
    }
    
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleLostFrag(JsonDocument &workingDoc, 
                                      uint8_t fromAddress) {
    WARNINGF("Dropping fragment body with no header received from %i",
             fromAddress);

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receive(uint timeout, uint8_t* fromAddress, bool shouldProxy) {
    int retryCount = receiveRetryCount;
    while (retryCount > 0) {
        if(!this->handshakeEstablished) {
            FragReceiveStatus handshakeAccepted = receiveFrag(timeout, shouldProxy, fromAddress);
            if (handshakeAccepted == FragReceiveStatus::Error) {
                retryCount--;
                continue;
            }
        }

        FragReceiveStatus status = receiveFrag(timeout, shouldProxy, fromAddress);
        switch (status) {
        case FragReceiveStatus::Complete:
            return true;
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
// getHandshakeResponse: Waits for handshake response (Accept/Deny) from peer
// Waits up to 6 seconds total for valid response from specified address
// Returns: true if ACCEPT received, false if DENY or timeout
bool Loom_LoRa::getHandshakeResponse(uint8_t handshakePartnerAddr) {
    if (!moduleInitialized) {
        return false;
    }

    uint8_t buf[MAX_MESSAGE_LENGTH] = {}; // buffer for received packet

    uint16_t recTimeout = 2000; // milliseconds per receive attempt
    const uint32_t deadline = millis() + 6000; // total timeout: 6 seconds
    uint8_t receiveAttempt = 1;

    uint8_t fromAddress;
    while(millis() < deadline) {
        // Try to receive packet
        bool recvStatus = receiveFromLoRa(buf, sizeof(buf), recTimeout, &fromAddress);

        if(!recvStatus) {
            unsigned long timeLeft = deadline - millis();
            receiveAttempt++;
            continue;
        }

        // Received packet - check if from correct source
        if(fromAddress != handshakePartnerAddr) {
            receiveAttempt++;
            continue;
        }

        StaticJsonDocument<300> tempDoc;

        // Parse MsgPack response
        auto err = deserializeMsgPack(tempDoc, (const char *)buf, sizeof(buf)); 
        if (err != DeserializationError::Ok) {
            receiveAttempt++;
            continue;
        }

        // Check response content
        if (tempDoc.containsKey("handshake")) {
            if (strcmp(tempDoc["handshake"], "Accept") == 0) {
                return true; // ACCEPTED
            } else if (strcmp(tempDoc["handshake"], "Deny") == 0) {
                return false; // DENIED
            } else {
                receiveAttempt++;
                continue;
            }
        } else {
            receiveAttempt++;
            continue;
        }
    }
    
    return false; // timeout elapsed without receiving valid handshake response
};
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
// conductHandshake: Initiates handshake with target device
// Returns: true if handshake accepted, false if denied or timed out
// Side effects: None - does NOT modify activePartner state. Caller responsible for state update.
bool Loom_LoRa::conductHandshake(const uint8_t destinationAddress) {
    // Build handshake request packet
    const uint8_t HANDSHAKE_SIZE = 100; // enough for the handshake key and string value
    StaticJsonDocument<HANDSHAKE_SIZE> handshakeDoc;
    handshakeDoc["handshake"] = "Request";

    uint8_t handshakesLeft = handshakeRetryCount;
    uint8_t attemptNumber = 1;
    
    // Retry loop: keep attempting until max retries exhausted
    while(handshakesLeft > 0) {
        // STEP 1: Send request to destination device
        bool handshakeTransmitStatus = sendFullPacket(handshakeDoc.as<JsonObject>(), destinationAddress);

        if (!handshakeTransmitStatus) {
            handshakesLeft--;
            attemptNumber++;
            continue;
        }
        
        // STEP 2: Wait for response (Accept/Deny) from destination
        bool handshakeAccepted = getHandshakeResponse(destinationAddress);
        
        if (handshakeAccepted) {
            return true; // SUCCESS - caller must update activePartner
        } else {
            handshakesLeft--;
            attemptNumber++;
        }
    }

    return false; // FAILURE - caller must NOT update activePartner
};
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::send(const uint8_t destinationAddress) {
    LOGF("[SEND] Called with destinationAddress=%i, current state: handshakeEstablished=%s, activePartner=%i, batchPacketsToSend=%i",
         destinationAddress, this->handshakeEstablished ? "true" : "false", this->activePartner, this->batchPacketsToSend);
    
    if(millis() - this->lastArrivalTime > 10000) { // if time since last message arrived is longer than 10 seconds
        LOG("[SEND] Handshake timeout elapsed, dropping active partner");
        this->handshakeEstablished = false;
        this->activePartner = -1;
    }

    if(this->handshakeEstablished && this->activePartner != destinationAddress) {
        LOGF("[SEND] WARNING: Already in handshake with %i but sending to %i. Dropping old handshake and re-establishing.",
             this->activePartner, destinationAddress);
        this->handshakeEstablished = false;
        this->activePartner = -1;
    }

    if(!this->handshakeEstablished) {
        LOGF("[SEND] Handshake not established. Initiating handshake sequence with device %i...", destinationAddress);
        bool handshakeResult = conductHandshake(destinationAddress);
        
        if(handshakeResult) {

            this->handshakeEstablished = true;
            this->activePartner = destinationAddress;
            LOGF("[SEND] Handshake SUCCEEDED with device %i. State: established=true, activePartner=%i", destinationAddress, this->activePartner);
        } else {
            // Handshake failed - state remains unchanged (do NOT set activePartner)
            this->handshakeEstablished = false;
            LOGF("[SEND] Handshake FAILED with device %i. NOT setting activePartner (critical fix). State: established=false, activePartner=%i (unchanged)",
                 destinationAddress, this->activePartner);
        }
    }

    bool sendStatus = false;
    if(this->handshakeEstablished && this->activePartner == destinationAddress) {
        LOGF("[SEND] State verified: activePartner=%i == dest=%i. Transmitting (batch: %s, remain: %i)",
             this->activePartner, destinationAddress, this->batchPacketsToSend > 0 ? "true" : "false", this->batchPacketsToSend);
        sendStatus = send(destinationAddress, manager->getDocument().as<JsonObject>());
        if(!sendStatus) {
            ERROR(F("[SEND] Packet transmission FAILED!"));
        }
    } else if(this->handshakeEstablished && this->activePartner != destinationAddress) {
        ERRORF("[SEND] STATE CORRUPTION: established=true but activePartner=%i != dest=%i!",
               this->activePartner, destinationAddress);
        this->handshakeEstablished = false;
        this->activePartner = -1;
        sendStatus = false;
    } else {
        ERRORF("[SEND] Aborting to %i - no handshake (est=%s, partner=%i)",
               destinationAddress, this->handshakeEstablished ? "true" : "false", this->activePartner);
        sendStatus = false;
    }

    if(this->batchPacketsToSend > 0) {
        this->batchPacketsToSend--;
        LOGF("[SEND] Decremented batchPacketsToSend. Remaining: %i", this->batchPacketsToSend);
    }

    if(this->batchPacketsToSend == 0) {
        LOGF("[SEND] Batch complete. Dropping handshake with %i", this->activePartner);
        this->handshakeEstablished = false;
        this->activePartner = -1;
    }

    LOGF("[SEND] Returning with status=%s. New state: handshakeEstablished=%s, activePartner=%i",
         sendStatus ? "true" : "false", this->handshakeEstablished ? "true" : "false", this->activePartner);
    return sendStatus;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::send(const uint8_t destinationAddress, 
                            JsonObject json) {
    if (!moduleInitialized) {
        ERROR(F("Module not initialized!"));
        return false;
    }

    this->lastArrivalTime = millis(); 

    bool transmissionStatus = false;
    if (measureMsgPack(json) > MAX_MESSAGE_LENGTH) {
        transmissionStatus = sendFragmentedPacket(json, destinationAddress);
    } else {
        transmissionStatus = sendFullPacket(json, destinationAddress);
    }

    return transmissionStatus;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool Loom_LoRa::sendBatch(const uint8_t destinationAddress) {
    bool status = false;
    LOGF("[SENDBATCH] Starting batch send to %i", destinationAddress);

    if (!moduleInitialized) {
        ERROR(F("[SENDBATCH] Module not initialized!"));
        return false;
    }

    if (!batchSD) {
        ERROR(F("[SENDBATCH] BatchSD module not set - cannot send batch"));
        return false;
    }

    if (!batchSD->shouldPublish()) {
        LOG(F("[SENDBATCH] BatchSD not ready to publish"));
        return true;
    }

    File fileOutput = batchSD->getBatch();
    int batchSize = batchSD->getBatchSize();
    // LOGF("[SENDBATCH] Batch size: %i packets", batchSize);

    this->batchPacketsToSend = batchSize;
    for (int i = 0; i < batchSize && fileOutput.available(); i++) {
        uint8_t packetBuf[2000];
        // read line from file into packetBuf
        int len = fileOutput.readBytesUntil('\n', packetBuf, 
                                            sizeof(packetBuf));

        if (!len) {
            // WARNINGF("[SENDBATCH] BatchSD data missing ending newline at packet %i/%i", i+1, batchSize);
            break;
        }

        // remove trailing carriage return if DOS line endings have been used
        if (packetBuf[len - 1] == '\r') {
            packetBuf[len - 1] = 0;
        }

        // deserialze packet into main document
        // LOGF("[SENDBATCH] Loaded packet %i/%i into manager document", i+1, batchSize);
        deserializeJson(manager->getDocument(), (const char *)packetBuf,
                        sizeof(packetBuf));

        status = send(destinationAddress);        
        if (status) {
            //LOGF("[SENDBATCH] Successfully transmitted packet (%i/%i)", i+1, batchSize);
        } else {
            //ERRORF("[SENDBATCH] Failed to transmit packet (%i/%i)", i+1, batchSize);
        }

        delay(500);

        Serial.println();
    }

    fileOutput.close();
    
    // Prevents stale batch counter from corrupting future sends
    this->batchPacketsToSend = 0;
    
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

