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
    const uint8_t receiveMaxRetries,
    const uint16_t retryTimeout
) : Module("LoRa"),
        manager(&manager), 
        radioDriver{RFM95_CS, RFM95_INT},
        deviceAddress(address),
        powerLevel(powerLevel),
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
        LOGF("Received handshake initiation from %i, sending response...", fromAddress);

        // build a light JSON doc to leverage sendFullPacket
        const uint8_t HANDSHAKE_SIZE = 300; // enough for the building a JSON doc from scratch w/100 bytes of raw data
        StaticJsonDocument<HANDSHAKE_SIZE> handshakeDoc;

        if (!this->handshakeEstablished) { // not in a handshake with another device
            LOG("Handshake request accepted!");
            handshakeDoc["handshake"] = "Accept";
            this->activePartner = fromAddress;
            this->handshakeEstablished = true;
            acceptHandshake = true;
        } else { // we're currently in a handshake with another device
            if (millis() - this->lastArrivalTime > 10000) { // drop if longer than 10 seconds passed
                LOG("Handshake timeout elapsed, dropping active partner and accepting new handshake request");
                frags.erase(activePartner);

                handshakeDoc["handshake"] = "Accept";
                this->activePartner = fromAddress;
                this->handshakeEstablished = true;
                acceptHandshake = true;
            }
            else {
                LOG("Handshake request denied, already in handshake with another device");
                handshakeDoc["handshake"] = "Deny";
                acceptHandshake = false;
            }
        }
    
        bool handshakeTransmitStatus = sendFullPacket(handshakeDoc.as<JsonObject>(), fromAddress);
        if (handshakeTransmitStatus) {
            LOG(F("Handshake response successfully sent!"));
        } else {
            ERROR(F("Failed to send handshake response! Handshake connection dropped."));

            // if we wanted to accept but couldn't respond, rollback the handshake connection.
            if(acceptHandshake == true) 
            {
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
        ERROR(F("LoRa module not initialized!"));
        return FragReceiveStatus::Error;
    }

    uint8_t buf[MAX_MESSAGE_LENGTH] = {};

    bool recvStatus = receiveFromLoRa(buf, sizeof(buf), timeout, fromAddress);
    if (!recvStatus) {
        return FragReceiveStatus::Error;
    }

    LOGF("Received packet fragment from %i", *fromAddress);

    StaticJsonDocument<300> tempDoc;

    // cast buf to const to avoid mutation
    auto err = deserializeMsgPack(tempDoc, (const char *)buf, sizeof(buf)); 
    if (err != DeserializationError::Ok) {
        ERRORF("Error occurred parsing MsgPack (raw bytes received): %s", err.c_str());
        return FragReceiveStatus::Error;
    }

    if (tempDoc.containsKey("handshake")) {
        bool acceptHandshake = false;
        acceptHandshake = handleHandshakeRequest(tempDoc.as<JsonObject>(), *fromAddress); 
        if(acceptHandshake) {
            LOG("Handshake established.");
            return FragReceiveStatus::Complete;
        }
        if(!acceptHandshake) {
            LOG("Handshake not accepted.");
            return FragReceiveStatus::Error; // early return to quit processing fragment early.
        }
    }

    // Handshake WALL - process won't parse any fragments that aren't from handshake relationship
    if(!this->handshakeEstablished || *fromAddress != this->activePartner) {
        LOGF("Currently in handshake with %i, dropping packet from %i", activePartner, *fromAddress);
        return FragReceiveStatus::Error;
    }
    
    // adjust last frag arrival time of an acceptable packet
    this->lastArrivalTime = millis(); 

    bool isReady = false;
    if (tempDoc.containsKey("batch_size")) {
        isReady = handleBatchHeader(tempDoc);
        LOGF("Batch header fragment handled");

    // NOTE: numPackets is referring to the number of fragments, not actual packets.
    } else if (tempDoc.containsKey("numPackets")) {
        isReady = handleFragHeader(tempDoc, *fromAddress);
        LOGF("fragment header fragment handled");

    } else if (this->handshakeEstablished && frags.find(*fromAddress) != frags.end()) {
        isReady = handleFragBody(tempDoc, *fromAddress);
        LOGF("fragment body fragment handled");
    
    } else if (tempDoc.containsKey("module")) {
        isReady = handleLostFrag(tempDoc, *fromAddress);
        LOGF("lost fragment handled");

    } else {
        isReady = handleSingleFrag(tempDoc);
        LOGF("single fragment handled");
    }

    if (isReady) {
        if (shouldProxy) {
            // Set this current device's name and instance number to match the sender of the received packets
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

        if(this->expectedOutstandingPackets == 0) {
            LOG("Received the final expected packet fragment of the communication. All fragments received!");
            this->handshakeEstablished = false; 
            this->activePartner = -1; 
        } else {
            LOG("More packet fragments expected to arrive");
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
    if(this->expectedOutstandingPackets == 0) {
        LOG("Received full single packet. All expected fragments received!");
        this->handshakeEstablished = false;
        this->activePartner = -1;
    } else {
        LOG("Received full complete packet. Expecting more fragments of communication to arrive.");
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

        // initial catch of handshake request, necessary to prevent the manager from displaying an additional
        //      log of it's internal document to the console upon only receiving a handshake request.
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
bool Loom_LoRa::getHandshakeResponse(uint8_t handshakePartnerAddr) {
    if (!moduleInitialized) {
        ERROR(F("LoRa module not initialized!"));
        return false;
    }

    uint8_t buf[MAX_MESSAGE_LENGTH] = {}; // stack allocated array (decays into pointer when passed to function)

    uint16_t recTimeout = 2000; // ms to wait for actual LoRa response for handshake
    const uint32_t deadline = millis() + 6000; // ms until we give up on waiting for handshake response

    uint8_t fromAddress;
    while(millis() < deadline) {
        bool recvStatus = receiveFromLoRa(buf, sizeof(buf), recTimeout, &fromAddress);

        if(!recvStatus) {
            LOG(F("No handshake response received, retrying..."));
            continue;
        }

        if(fromAddress != handshakePartnerAddr) {
            LOGF("Received packet from %i while waiting for handshake response from %i, ignoring...", fromAddress, handshakePartnerAddr);
            continue;
        }

        LOGF("Received packet fragment from %i", fromAddress);

        StaticJsonDocument<300> tempDoc;

        // cast buf to const to avoid mutation
        auto err = deserializeMsgPack(tempDoc, (const char *)buf, sizeof(buf)); 
        if (err != DeserializationError::Ok) {
            ERRORF("Error occurred parsing MsgPack (raw bytes received): %s", err.c_str());
            return false;
        }

        if (tempDoc.containsKey("handshake") && 
            strcmp(tempDoc["handshake"], "Accept") == 0) {
            return true;
        } else {
            return false;
        }
    }
    return false; // timeout elapsed without receiving valid handshake response
};
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::conductHandshake(const uint8_t destinationAddress) {
    // build a light JSON doc to leverage sendFullPacket
    const uint8_t HANDSHAKE_SIZE = 100; // enough for the handshake key and string value
    StaticJsonDocument<HANDSHAKE_SIZE> handshakeDoc;
    handshakeDoc["handshake"] = "Request";

    const uint8_t handshakeRetryCount = 3; // amount of times to retry the handshake connection
    uint8_t handshakesLeft = handshakeRetryCount;
    while(handshakesLeft > 0) {
        // send handshake request
        bool handshakeTransmitStatus = sendFullPacket(handshakeDoc.as<JsonObject>(), destinationAddress);

        if (!handshakeTransmitStatus) {
            ERROR(F("Failed to transmit handshake packet!"));
            handshakesLeft--;
            continue;
        }

        // check handshake response
        bool handshakeAccepted = getHandshakeResponse(destinationAddress);
        if (handshakeAccepted) {
            LOG(F("Handshake successfully established!"));
            return true;
        } else {
            ERROR(F("Handshake not accepted!"));
            handshakesLeft--;
        }
    }

    return false; // ran out of retries
};
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::send(const uint8_t destinationAddress) {
    if(millis() - this->lastArrivalTime > 10000) { // if time since last message arrived is longer than 10 seconds
        LOG("Handshake timeout elapsed, dropping active partner");
        this->handshakeEstablished = false;
        this->activePartner = -1;
    }

    if(!this->handshakeEstablished) {
        this->handshakeEstablished = conductHandshake(destinationAddress);
        this->activePartner = destinationAddress;
    }

    bool sendStatus = false;
    if(this->handshakeEstablished) {
        LOG(F("Proceeding with full message sending since handshake was accepted"));
        sendStatus = send(destinationAddress, manager->getDocument().as<JsonObject>());
    } else {
        ERROR(F("Aborting Send since no completed handshake!"));
        sendStatus = false;
    }

    if(this->batchPacketsToSend == 0) {
        LOG("Completed message sending process. Dropping any active Handshake.")
        this->handshakeEstablished = false;
        this->activePartner = -1;
    }

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

    this->batchPacketsToSend = batchSize;
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

        this->batchPacketsToSend--;

        delay(500);

        Serial.println();
    }

    fileOutput.close();
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

