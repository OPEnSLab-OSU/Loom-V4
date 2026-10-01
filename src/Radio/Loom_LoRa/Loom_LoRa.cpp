#include "Loom_WarningGuards.h"

#include "Loom_LoRa.h"
#include "Hardware/Loom_BatchSD/Loom_BatchSD.h"
#include "Loom_Manager.h"
#include "Utilities/Loom_LoRaPacketHeader.h"
#include "Utilities/Loom_HeartbeatPayload.h"
#include "Utilities/Loom_SDUtils.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include "ArduinoJson.hpp"
#include "FatLib/ArduinoFiles.h"
LOOM_EXTERNAL_INCLUDE_END
#include "Logger.h"
#include "Module.h"
#include <cstdint>
#include <cstdio>

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LoRa::Loom_LoRa(Manager &manager, const uint8_t address, const uint8_t powerLevel,
                     const uint8_t sendMaxRetries, const uint8_t receiveMaxRetries,
                     const uint16_t retryTimeout, Mode mode)
    : Module("LoRa"), manager(&manager), radioDriver{RFM95_CS, RFM95_INT},
      radioManager(radioDriver, address), deviceAddress(address), powerLevel(powerLevel),
      sendRetryCount(sendMaxRetries), receiveRetryCount(receiveMaxRetries),
      retryTimeout(retryTimeout), transmissionMode(mode) {
    this->manager->registerModule(this);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LoRa::Loom_LoRa(Manager &manager, const uint8_t powerLevel, const uint8_t retryCount,
                     const uint16_t retryTimeout, Mode mode)
    : Loom_LoRa(manager, manager.get_instance_num(), powerLevel, retryCount, retryCount,
                retryTimeout, mode) {}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::power_up() {
    if (batchSD) {
        poweredUp = batchSD->shouldPowerModem();
    }

    if (poweredUp) {
        radioDriver.available();
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::power_down() {
    if (poweredUp) {
        radioDriver.sleep();
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::package() {
    if (!moduleInitialized) {
        return;
    }

    JsonObject json = manager->get_data_object(getModuleName());
    json["RSSI"] = signalStrength;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::setAddress(const uint8_t newAddress) {
    deviceAddress = newAddress;
    radioManager.setThisAddress(newAddress);
    radioDriver.sleep();
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::loadScheduleFromJSON(const char *json, loomLoRa::Schedule &value) {
    if (json == nullptr) {
        return false;
    }
    StaticJsonDocument<256> settings;
    if (deserializeJson(settings, json) || settings.overflowed() ||
        !settings["window_length"].is<uint32_t>() || !settings["num_devices"].is<unsigned int>()) {
        return false;
    }
    const unsigned int devices = settings["num_devices"].as<unsigned int>();
    if (devices > 16 ||
        (!settings["cycle_seconds"].isNull() && !settings["cycle_seconds"].is<uint32_t>()) ||
        (!settings["early_wake_seconds"].isNull() &&
         !settings["early_wake_seconds"].is<uint32_t>())) {
        return false;
    }
    if (!value.configure(settings["window_length"].as<uint32_t>(), static_cast<uint8_t>(devices),
                         settings["cycle_seconds"] | 3600UL,
                         settings["early_wake_seconds"] | 60UL)) {
        return false;
    }
    setSchedule(value);
    restrictToGroup();
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
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
    if (groupRestricted && !loomLoRa::Schedule::sameGroup(deviceAddress, *fromAddress)) {
        return FragReceiveStatus::Incomplete; // Drop before touching a packet or partial state.
    }
    expireReceiveState();

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
        isReady = handleBatchHeader(workingDoc, *fromAddress);
    } else if (workingDoc.containsKey("numPackets")) {
        isReady = handleFragHeader(workingDoc, *fromAddress);
    } else if (workingDoc.containsKey("module")) {
        // Only a module object is a fragment body. A fresh full packet or heartbeat must
        // never be appended to an abandoned packet merely because its sender is the same.
        isReady = fragmentActive && fragmentSender == *fromAddress
                      ? handleFragBody(workingDoc, *fromAddress)
                      : handleLostFrag(workingDoc, *fromAddress);
    } else {
        if (fragmentActive && fragmentSender == *fromAddress) {
            WARNING(F("Fresh LoRa packet replaces this sender's incomplete fragments"));
            resetFragmentState();
        }
        isReady = handleSingleFrag(workingDoc);
    }

    if (isReady) {
        if (shouldProxy && workingDoc.containsKey("id")) {
            const JsonObjectConst identity = workingDoc["id"].as<JsonObjectConst>();
            if (!loomHeartbeat::validIdentity(identity)) {
                ERROR(F("LoRa proxy identity is incomplete or too long; packet rejected."));
                workingDoc.clear();
                return FragReceiveStatus::Error;
            }
            // Validate both fields before changing either. Truncating only the name could
            // otherwise publish this packet under a different device's Mongo topic.
            manager->set_device_name(identity["name"].as<const char *>());
            manager->set_instance_num(identity["instance"].as<int>());
        }

        if (manager->getDocument()["type"] == "data") {
            batchReceive.complete(*fromAddress, millis());
        }
        return FragReceiveStatus::Complete;
    } else {
        return FragReceiveStatus::Incomplete;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleBatchHeader(JsonDocument &tempDoc, uint8_t fromAddress) {
    int batch_size = tempDoc["batch_size"];
    if (!tempDoc["batch_size"].is<int>() || batch_size <= 0 || batch_size > 255) {
        ERROR(F("LoRa batch header contains an invalid packet count"));
        return false;
    }
    LOGF("Received batch header, expecting %i packets", batch_size);
    batchReceive.start(fromAddress, static_cast<uint8_t>(batch_size), millis());
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleFragHeader(JsonDocument &workingDoc, uint8_t fromAddress) {
    int expectedFragCount = workingDoc["numPackets"].as<int>();
    if (!workingDoc["numPackets"].is<int>() || expectedFragCount <= 0 ||
        expectedFragCount > MAX_FRAGMENT_COUNT || !workingDoc["contents"].is<JsonArray>() ||
        workingDoc["contents"].size() != 0 ||
        !loomHeartbeat::validIdentity(workingDoc["id"].as<JsonObjectConst>())) {
        ERRORF("Rejecting LoRa fragment count %i", expectedFragCount);
        return false;
    }

    if (fragmentActive) {
        WARNINGF("Dropping incomplete fragmented packet from %i", fragmentSender);
        resetFragmentState();
    }

    if (fragmentWorking.capacity() < MAX_JSON_SIZE) {
        fragmentWorking = FragmentDocument(MAX_JSON_SIZE, loomMemory::JsonAllocator(receivePool));
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
    lastFragmentProgress = millis();
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleFragBody(JsonDocument &workingDoc, uint8_t fromAddress) {
    if (!fragmentActive || fragmentSender != fromAddress || remainingFragments <= 0) {
        WARNINGF("Dropping unexpected fragmented packet body from %i", fromAddress);
        return false;
    }
    if (!workingDoc["module"].is<const char *>() || !workingDoc["data"].is<JsonObject>()) {
        ERROR(F("LoRa fragment body is missing its module name or data object"));
        resetFragmentState();
        return false;
    }

    JsonArray contents = fragmentWorking["contents"].as<JsonArray>();
    if (contents.isNull() || !contents.add(workingDoc) || fragmentWorking.overflowed()) {
        ERROR(F("LoRa fragmented packet exceeds MAX_JSON_SIZE"));
        resetFragmentState();
        return false;
    }

    remainingFragments--;
    lastFragmentProgress = millis();

    if (remainingFragments == 0) {
        // overwrite the manager document by deep-copying the finalized packet
        manager->getDocument().set(fragmentWorking);
        const bool overflowed = manager->getDocument().overflowed();
        resetFragmentState();

        if (overflowed) {
            ERROR(F("Completed LoRa packet exceeds the Manager JSON capacity"));
            return false;
        }
        return handleSingleFrag(manager->getDocument());
    }
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::resetFragmentState() {
    fragmentWorking.clear();
    remainingFragments = 0;
    fragmentSender = 0;
    fragmentActive = false;
    lastFragmentProgress = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::expireReceiveState() {
    const uint32_t now = millis();
    if (fragmentActive &&
        static_cast<uint32_t>(now - lastFragmentProgress) >= RECEIVE_IDLE_LIMIT_MS) {
        WARNINGF("Discarding stalled LoRa fragments from %i", fragmentSender);
        resetFragmentState();
    }
    if (batchReceive.expire(now, RECEIVE_IDLE_LIMIT_MS)) {
        WARNING(F("LoRa batch stalled; releasing the outstanding packet estimate"));
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleSingleFrag(JsonDocument &workingDoc) {
    // receiveFrag() already parsed this packet into the Manager document.
    // send(address, JsonObject) also supports custom objects and legacy heartbeats without
    // contents. Keep that API; proxy identity is updated only when its fields are present.
    return !workingDoc.overflowed() && workingDoc.is<JsonObject>() && workingDoc.size() > 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleLostFrag(JsonDocument &workingDoc, uint8_t fromAddress) {
    (void)workingDoc;
    WARNINGF("Dropping fragmented packet body with no header received from %i", fromAddress);
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receive(uint timeout, uint8_t *fromAddress, bool shouldProxy) {
    if (fromAddress == nullptr) {
        ERROR(F("LoRa receive requires a sender-address output pointer."));
        return false;
    }

    *fromAddress = 0;
    expireReceiveState();
    const uint32_t started = millis();
    int errorsRemaining = receiveRetryCount > 0 ? receiveRetryCount : 1;
    // Two independent bounds: total wall time and number of datagrams. Even a stream of
    // incomplete headers cannot monopolize the hub. timeout == 0 polls available traffic.
    for (int inspected = 0; inspected < MAX_DATAGRAMS_PER_RECEIVE; ++inspected) {
        const uint32_t elapsed = static_cast<uint32_t>(millis() - started);
        if (timeout > 0 && elapsed >= timeout) {
            break;
        }
        // RadioHead's blocking timeout is uint16_t; do not narrow a longer caller budget.
        const uint32_t remaining = timeout > 0 ? timeout - elapsed : 0;
        const uint wait = remaining > UINT16_MAX ? UINT16_MAX : remaining;
        const FragReceiveStatus status = receiveFrag(wait, shouldProxy, fromAddress);
        if (status == FragReceiveStatus::Complete) {
            return true;
        }
        if (status == FragReceiveStatus::Error) {
            --errorsRemaining;
            if (errorsRemaining == 0 || timeout == 0) {
                break;
            }
        }
    }
    expireReceiveState();
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receive(uint timeout, bool shouldProxy) {
    uint8_t fromAddress;
    return receive(timeout, &fromAddress, shouldProxy);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::transmitToLoRa(JsonObjectConst json, uint8_t destinationAddress) {
    if (groupRestricted && (destinationAddress == 0xff ||
                            !loomLoRa::Schedule::sameGroup(deviceAddress, destinationAddress))) {
        ERROR(F("Grouped LoRa requires a unicast destination in this address group."));
        return false; // Also covers batch/fragment headers that bypass the public send() helper.
    }
    uint8_t buffer[MAX_MESSAGE_LENGTH] = {};
    const size_t expected = measureMsgPack(json);
    if (json.isNull() || expected == 0 || expected > sizeof(buffer) ||
        serializeMsgPack(json, buffer, sizeof(buffer)) != expected) {
        ERROR(F("LoRa packet is invalid or too large; refusing a truncated MsgPack send"));
        return false;
    }

    // Legacy mode retains padded framing. The new heartbeat-only opt-in sends just its
    // encoded bytes, reducing airtime without changing the MessagePack field names.
    const uint8_t wireLength = transmissionMode == Mode::HeartbeatOnly
                                   ? static_cast<uint8_t>(expected)
                                   : static_cast<uint8_t>(sizeof(buffer));
    const bool status = radioManager.sendtoWait(buffer, wireLength, destinationAddress);
    if (!status) {
        ERROR(F("Failed to send packet to specified address!"));
        return false;
    }

    LOG(F("Successfully transmitted packet!"));
    signalStrength = radioDriver.lastRssi();
    radioDriver.sleep();
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendFullPacket(JsonObject json, uint8_t destinationAddress) {
    return transmitToLoRa(json, destinationAddress);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendFragmentedPacket(JsonObject json, uint8_t destinationAddress) {
    LOG(F("Packet was greater than the maximum packet length; the packet will be fragmented"));
    bool status = false;

    status = json.containsKey("contents");
    if (!status) {
        ERROR(F("JSON data is malformed and cannot be fragmented"));
        return false;
    }
    int numFrags = json["contents"].size();
    if (numFrags <= 0 || numFrags > MAX_FRAGMENT_COUNT) {
        ERROR(F("LoRa packet contains an unsupported fragment count"));
        return false;
    }
    // Preflight every body before announcing a packet the receiver could never assemble.
    for (JsonVariant fragment : json["contents"].as<JsonArray>()) {
        if (!fragment.is<JsonObject>() || measureMsgPack(fragment) > MAX_MESSAGE_LENGTH) {
            ERROR(F("A LoRa module fragment exceeds the radio payload limit"));
            return false;
        }
    }

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
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendPacketHeader(JsonObject json, uint8_t destinationAddress) {
    StaticJsonDocument<loomLoRa::FRAGMENT_HEADER_CAPACITY> sendDoc;
    if (!loomLoRa::buildFragmentHeader(sendDoc, json)) {
        ERROR(F("LoRa fragment header exceeds its JSON capacity"));
        return false;
    }
    return transmitToLoRa(sendDoc.as<JsonObject>(), destinationAddress);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::send(const uint8_t destinationAddress) {
    if (transmissionMode == Mode::HeartbeatOnly) {
        return sendHeartbeat(destinationAddress);
    }
    if (!manager->isPacketValid()) {
        ERROR(F("Refusing to send an empty or overflowed JSON packet."));
        return false;
    }
    return send(destinationAddress, manager->getDocument().as<JsonObject>());
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendHeartbeat(const uint8_t destinationAddress) {
    StaticJsonDocument<256> heartbeat;
    if (!loomHeartbeat::buildStatusPayload(heartbeat, manager->get_device_name(),
                                           manager->get_instance_num())) {
        return false;
    }
    return sendHeartbeat(destinationAddress, heartbeat);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendHeartbeat(const uint8_t destinationAddress, const JsonDocument &heartbeat) {
    const JsonObjectConst packet = heartbeat.as<JsonObjectConst>();
    if (!moduleInitialized || !loomJsonIsComplete(heartbeat) ||
        !loomHeartbeat::isStatusPayload(packet) || measureMsgPack(packet) > MAX_MESSAGE_LENGTH) {
        ERROR(F("Cannot send an unavailable, incomplete or oversized LoRa heartbeat."));
        return false;
    }
    return transmitToLoRa(packet, destinationAddress);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::send(const uint8_t destinationAddress, JsonObject json) {
    if (transmissionMode == Mode::HeartbeatOnly &&
        (!loomHeartbeat::isStatusPayload(json) || measureMsgPack(json) > MAX_MESSAGE_LENGTH)) {
        ERROR(F("Heartbeat-only LoRa requires one status frame with no measurements."));
        return false;
    }
    if (groupRestricted && !loomLoRa::Schedule::sameGroup(deviceAddress, destinationAddress)) {
        ERROR(F("LoRa destination is outside the configured address group."));
        return false;
    }
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
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendBatch(const uint8_t destinationAddress) {
    if (transmissionMode == Mode::HeartbeatOnly) {
        WARNING(F("Heartbeat-only LoRa does not transmit or clear an SD batch."));
        return false;
    }
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

    for (int i = 0; i < recordsToSend; ++i) {
        uint32_t lineStart = 0;
        size_t lineLength = 0;
        bool terminated = false;
        const loomSD::RecordResult record =
            loomSD::nextRecord(fileOutput, lineStart, lineLength, MAX_JSON_SIZE, &terminated);
        if (record != loomSD::RecordResult::Ready || !terminated ||
            !fileOutput.seekSet(lineStart)) {
            ERROR(F("Batch record is missing, damaged or unterminated; retaining the batch."));
            fileOutput.close();
            return false;
        }
        // Parse one JSON value directly from the SD stream. The old path reserved a 2 KB local
        // packet buffer on the SAMD21 stack and parsed uninitialized bytes after short lines.
        const DeserializationError error = deserializeJson(manager->getDocument(), fileOutput);
        if (error != DeserializationError::Ok ||
            !loomSD::finishJsonRecord(fileOutput, lineStart + lineLength) ||
            fileOutput.getError() != 0) {
            ERRORF("Batch packet %i rejected: JSON=%s; check row boundary and SD read status.",
                   i + 1, error.c_str());
            // A failed SD read may leave available() positive forever. Do not drain a
            // damaged record; stop this replay and keep the batch for diagnosis/retry.
            fileOutput.close();
            return false;
        }

        ++recordsSeen;
        const bool status = send(destinationAddress);
        if (status) {
            LOGF("Successfully transmitted packet (%i/%i)", i + 1, recordsToSend);
        } else {
            ERRORF("Failed to transmit packet (%i/%i)", i + 1, recordsToSend);
            fileOutput.close();
            return false; // Stop wasting airtime; replay the retained batch on a later attempt.
        }

        delay(500);

        Serial.println();
    }

    uint32_t extraStart = 0;
    size_t extraLength = 0;
    const bool recordCountMatches = recordsSeen == recordsToSend &&
                                    loomSD::nextRecord(fileOutput, extraStart, extraLength,
                                                       MAX_JSON_SIZE) == loomSD::RecordResult::End;
    const bool readSucceeded = fileOutput.getError() == 0;
    const bool closed = fileOutput.close();
    if (!readSucceeded || !closed) {
        ERROR(F("Batch reader failed or did not close; retaining the upload queue."));
        return false;
    }
    if (!recordCountMatches) {
        ERRORF("BatchSD record/count mismatch (%i file records inspected, %i counted); retaining "
               "the file.",
               recordsSeen, recordsToSend);
        return false;
    }

    if (recordsSeen == 0) {
        return false;
    }

    if (!batchSD->markPublished()) {
        ERROR(F("Batch was sent, but its SD file could not be cleared; it will be retried."));
        return false;
    }
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receiveBatch(uint timeout, int *numberOfPackets) {
    uint8_t fromAddress;
    return receiveBatch(timeout, numberOfPackets, &fromAddress);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receiveBatch(uint timeout, int *numberOfPackets, uint8_t *fromAddress) {
    if (numberOfPackets == nullptr || fromAddress == nullptr) {
        ERROR(F("LoRa batch receive requires non-null output pointers."));
        return false;
    }
    bool status = receive(timeout, fromAddress, true);
    if (!status && timeout > 0) {
        // The missing node may never return. Release the caller's do/while loop; saved data
        // on the sender remains its responsibility and can be replayed on a later attempt.
        batchReceive.cancel();
    }
    *numberOfPackets = batchReceive.remaining();
    return status;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
