
#include "Loom_LoRa.h"
#include "ArduinoJson.hpp"
#include "FatLib/ArduinoFiles.h"
#include "Logger.h"
#include "Module.h"
#include <cstdint>
#include <cstdio>

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LoRa::Loom_LoRa(Manager &manager, const uint8_t address, const uint8_t powerLevel,
                     const uint8_t sendMaxRetries, const uint8_t receiveMaxRetries,
                     const uint16_t retryTimeout)
    : Module("LoRa"), manager(&manager), radioDriver{RFM95_CS, RFM95_INT},
      radioManager(radioDriver, address), deviceAddress(address), powerLevel(powerLevel),
      sendRetryCount(sendMaxRetries), receiveRetryCount(receiveMaxRetries),
      retryTimeout(retryTimeout), expectedOutstandingPackets(0) {
    this->manager->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LoRa::Loom_LoRa(Manager &manager, const uint8_t powerLevel, const uint8_t retryCount,
                     const uint16_t retryTimeout)
    : Loom_LoRa(manager, manager.get_instance_num(), powerLevel, retryCount, retryCount,
                retryTimeout) {}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::initialize() {
    // Set CS pin as pull up
    pinMode(RFM95_CS, INPUT_PULLUP);

    // Reset the radio
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);

    // Initialize the radio manager
    if (radioManager.init()) {
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
    radioManager.setTimeout(retryTimeout);

    // Set retry attempts
    LOGF("Transmit retry count set to: %i", sendRetryCount);
    radioManager.setRetries(sendRetryCount);

    // Print the set address of the device
    LOGF("Address set to: %i", radioManager.thisAddress());

    // https://cdn.sparkfun.com/assets/a/e/7/e/b/RFM95_96_97_98W.pdf, Page 22

    // Set bandwidth
    radioDriver.setSignalBandwidth(125000);

    // Higher spreading factors give us more range
    radioDriver.setSpreadingFactor(7);

    // Coding rate should be 4/5
    radioDriver.setCodingRate4(5);
    radioDriver.sleep();
    moduleInitialized = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::power_up() {
    if (batchSD) {
        int currentBatch = batchSD->getCurrentBatch();
        int batchSize = batchSD->getBatchSize();
        poweredUp = batchSize > 0 && currentBatch >= batchSize - 1;
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
    radioManager.setThisAddress(newAddress);
    radioDriver.sleep();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receiveFromLoRa(uint8_t *buf, uint8_t bufferCapacity, uint8_t &receivedLength,
                                uint timeout, uint8_t *fromAddress) {
    bool status = true;

    memset(buf, 0, bufferCapacity);
    receivedLength = bufferCapacity;

    LOG(F("Waiting for message..."));

    if (timeout) {
        status = radioManager.recvfromAckTimeout(buf, &receivedLength, timeout, fromAddress);
    } else {
        status = radioManager.recvfromAck(buf, &receivedLength, fromAddress);
    }

    if (!status) {
        receivedLength = 0;
        WARNING(F("No message received"));
    }

    radioDriver.sleep();
    return status;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
FragReceiveStatus Loom_LoRa::receiveFrag(uint timeout, bool shouldProxy, uint8_t *fromAddress) {
    if (!moduleInitialized) {
        ERROR(F("LoRa module not initialized!"));
        return FragReceiveStatus::Error;
    }

    uint8_t buf[MAX_MESSAGE_LENGTH] = {};
    uint8_t receivedLength = 0;

    bool recvStatus = receiveFromLoRa(buf, sizeof(buf), receivedLength, timeout, fromAddress);
    if (!recvStatus) {
        return FragReceiveStatus::Error;
    }

    LOGF("Received packet from %i", *fromAddress);

    // Reuse the Manager's existing 2 KB document as the receive workspace. This removes the old
    // 300-byte stack document and parses only the bytes RadioHead actually received.
    DynamicJsonDocument &workingDoc = manager->getDocument();
    workingDoc.clear();
    auto err = deserializeMsgPack(workingDoc, static_cast<const uint8_t *>(buf), receivedLength);
    if (err != DeserializationError::Ok) {
        ERRORF("Error occurred parsing MsgPack: %s", err.c_str());
        workingDoc.clear();
        return FragReceiveStatus::Error;
    }

    bool isReady = false;
    if (workingDoc.containsKey("batch_size")) {
        isReady = handleBatchHeader(workingDoc);

    } else if (workingDoc.containsKey("numPackets")) {
        isReady = handleFragHeader(workingDoc, *fromAddress);

    } else if (fragmentActive && fragmentSender == *fromAddress) {
        isReady = handleFragBody(workingDoc, *fromAddress);

    } else if (workingDoc.containsKey("module")) {
        isReady = handleLostFrag(workingDoc, *fromAddress);

    } else {
        isReady = handleSingleFrag(workingDoc);
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
bool Loom_LoRa::handleBatchHeader(JsonDocument &tempDoc) {
    int batch_size = tempDoc["batch_size"];
    if (batch_size <= 0 || batch_size > 255) {
        ERROR(F("LoRa batch header contains an invalid packet count"));
        return false;
    }
    LOGF("Received batch header, expecting %i packets", batch_size);
    expectedOutstandingPackets += batch_size;
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleFragHeader(JsonDocument &workingDoc, uint8_t fromAddress) {
    int expectedFragCount = workingDoc["numPackets"].as<int>();
    if (expectedFragCount <= 0 || expectedFragCount > MAX_FRAGMENT_COUNT) {
        ERRORF("Rejecting LoRa fragment count %i", expectedFragCount);
        return false;
    }

    if (fragmentActive) {
        WARNINGF("Dropping incomplete fragmented packet from %i", fragmentSender);
        resetFragmentState();
    }

    if (fragmentWorking.capacity() < MAX_JSON_SIZE) {
        fragmentWorking = DynamicJsonDocument(MAX_JSON_SIZE);
        if (fragmentWorking.capacity() < MAX_JSON_SIZE) {
            ERROR(F("Unable to allocate the LoRa fragment workspace"));
            resetFragmentState();
            return false;
        }
    }

    workingDoc.remove("numPackets");
    fragmentWorking.set(workingDoc);
    if (fragmentWorking.overflowed()) {
        ERROR(F("LoRa fragment header exceeds MAX_JSON_SIZE"));
        resetFragmentState();
        return false;
    }

    remainingFragments = expectedFragCount;
    fragmentSender = fromAddress;
    fragmentActive = true;

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleFragBody(JsonDocument &workingDoc, uint8_t fromAddress) {
    if (!fragmentActive || fragmentSender != fromAddress || remainingFragments <= 0) {
        WARNINGF("Dropping unexpected fragmented packet body from %i", fromAddress);
        return false;
    }

    JsonArray contents = fragmentWorking["contents"].as<JsonArray>();
    if (contents.isNull() || !contents.add(workingDoc) || fragmentWorking.overflowed()) {
        ERROR(F("LoRa fragmented packet exceeds MAX_JSON_SIZE"));
        resetFragmentState();
        return false;
    }

    remainingFragments--;

    if (remainingFragments == 0) {
        // overwrite the manager document by deep-copying the finalized packet
        manager->getDocument().set(fragmentWorking);
        const bool overflowed = manager->getDocument().overflowed();
        resetFragmentState();

        if (overflowed) {
            ERROR(F("Completed LoRa packet exceeds the Manager JSON capacity"));
            return false;
        }

        return true;
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::resetFragmentState() {
    fragmentWorking.clear();
    remainingFragments = 0;
    fragmentSender = 0;
    fragmentActive = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleSingleFrag(JsonDocument &workingDoc) {
    // receiveFrag() already parsed this packet into the Manager document.
    return !workingDoc.overflowed();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleLostFrag(JsonDocument &workingDoc, uint8_t fromAddress) {
    (void)workingDoc;
    WARNINGF("Dropping fragmented packet body with no header received from %i", fromAddress);

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receive(uint timeout, uint8_t *fromAddress, bool shouldProxy) {
    if (fromAddress == nullptr) {
        ERROR(F("LoRa receive requires a sender-address output pointer."));
        return false;
    }

    int retryCount = receiveRetryCount;
    while (retryCount > 0) {
        FragReceiveStatus status = receiveFrag(timeout, shouldProxy, fromAddress);

        switch (status) {
        case FragReceiveStatus::Incomplete:
            break;
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

    status = radioManager.sendtoWait(buffer, sizeof(buffer), destinationAddress);
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
bool Loom_LoRa::sendFragmentedPacket(JsonObject json, uint8_t destinationAddress) {
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
        LOGF("Sending fragmented packet (%i/%i)...", i + 1, numFrags);

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
bool Loom_LoRa::sendPacketHeader(JsonObject json, uint8_t destinationAddress) {
    StaticJsonDocument<MAX_MESSAGE_LENGTH * 2> sendDoc;

    sendDoc["type"] = json["type"].as<const char *>();
    sendDoc["numPackets"] = json["contents"].size();

    JsonObject objId = sendDoc.createNestedObject("id");
    objId["name"] = json["id"]["name"].as<const char *>();
    objId["instance"] = json["id"]["instance"].as<int>();

    sendDoc.createNestedArray("contents");

    if (!json["timestamp"].isNull()) {
        JsonObject objTimestamp = sendDoc.createNestedObject("timestamp");
        objTimestamp["time_utc"] = json["timestamp"]["time_utc"].as<const char *>();
        objTimestamp["time_local"] = json["timestamp"]["time_local"].as<const char *>();
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
bool Loom_LoRa::send(const uint8_t destinationAddress, JsonObject json) {
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
    bool allSucceeded = true;
    bool attempted = false;

    if (!moduleInitialized) {
        ERROR(F("Module not initialized!"));
        return false;
    }

    if (!batchSD) {
        ERROR(F("BatchSD module not set - cannot send batch"));
        return false;
    }

    if (batchSD->getBatchSize() <= 0) {
        ERROR(F("Invalid BatchSD configuration - cannot send batch"));
        return false;
    }

    if (!batchSD->shouldPublish()) {
        LOG(F("BatchSD not ready to publish"));
        return true;
    }

    File fileOutput = batchSD->openBatch();
    if (!fileOutput) {
        ERROR(F("Unable to open the BatchSD file"));
        return false;
    }

    const int recordsToSend = batchSD->getCurrentBatch();
    int recordsSeen = 0;

    for (int i = 0; i < recordsToSend && fileOutput.available(); i++) {
        recordsSeen++;
        // Parse one JSON value directly from the SD stream. The old path reserved a 2 KB local
        // packet buffer on the SAMD21 stack and parsed uninitialized bytes after short lines.
        const DeserializationError error = deserializeJson(manager->getDocument(), fileOutput);
        if (error != DeserializationError::Ok) {
            ERRORF("Failed to parse batch packet %i: %s", i + 1, error.c_str());
            while (fileOutput.available() && fileOutput.read() != '\n') {
            }
            allSucceeded = false;
            continue;
        }

        while (fileOutput.peek() == '\r' || fileOutput.peek() == '\n')
            fileOutput.read();

        attempted = true;
        const bool status = send(destinationAddress);
        allSucceeded = status && allSucceeded;
        if (status) {
            LOGF("Successfully transmitted packet (%i/%i)", i + 1, recordsToSend);
        } else {
            ERRORF("Failed to transmit packet (%i/%i)", i + 1, recordsToSend);
        }

        delay(500);

        Serial.println();
    }

    const bool recordCountMatches = recordsSeen == recordsToSend && !fileOutput.available();
    fileOutput.close();
    if (!recordCountMatches) {
        ERRORF("BatchSD record/count mismatch (%i file records inspected, %i counted); retaining "
               "the file.",
               recordsSeen, recordsToSend);
        return false;
    }

    if (!attempted || !allSucceeded)
        return false;

    if (!batchSD->markPublished()) {
        ERROR(F("Batch was sent, but its SD file could not be cleared; it will be retried."));
        return false;
    }
    return true;
}

bool Loom_LoRa::receiveBatch(uint timeout, int *numberOfPackets) {
    uint8_t fromAddress;
    return receiveBatch(timeout, numberOfPackets, &fromAddress);
}

bool Loom_LoRa::receiveBatch(uint timeout, int *numberOfPackets, uint8_t *fromAddress) {
    if (numberOfPackets == nullptr || fromAddress == nullptr) {
        ERROR(F("LoRa batch receive requires non-null output pointers."));
        return false;
    }
    bool status = receive(timeout, fromAddress, true);
    *numberOfPackets = expectedOutstandingPackets;
    return status;
}
