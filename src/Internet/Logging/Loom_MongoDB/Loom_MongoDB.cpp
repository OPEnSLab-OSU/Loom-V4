#include "Loom_MongoDB.h"
#include "Utilities/Loom_HeartbeatPayload.h"
#include "Hardware/Loom_BatchSD/Loom_BatchSD.h"
#include "Sensors/Loom_Analog/Loom_Analog.h"
#include "Logger.h"
#include "Utilities/Loom_SDUtils.h"
#include "Utilities/Loom_MQTTUtils.h"

namespace {
bool configStringIsComplete(JsonVariantConst value) {
    const JsonString text = value.as<JsonString>();
    return text.isNull() || memchr(text.c_str(), '\0', text.size()) == nullptr;
}
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MongoDB::Loom_MongoDB(Manager &man, NetworkComponent &internet_client,
                           const char *broker_address, int broker_port, const char *database_name,
                           const char *broker_user, const char *broker_pass,
                           const char *projectServer)
    : MQTTComponent("MongoDB", internet_client), manInst(&man) {
    // A hub may proxy many node names, but its MQTT connection belongs to this physical Feather.
    // The default millis()-based Arduino client ID can repeat across devices/reboots.
    setClientID(man.get_serial_num());
    if (broker_address == nullptr || broker_address[0] == '\0' ||
        strlen(broker_address) >= sizeof(address) || broker_port < 1 || broker_port > 65535 ||
        !loomMQTT::validTopic(database_name, sizeof(this->database_name), true) ||
        (projectServer != nullptr && projectServer[0] != '\0' &&
         !loomMQTT::validTopic(projectServer, sizeof(this->projectServer), true))) {
        moduleInitialized = false;
    }
    /* MQTT Connection parameters */
    strncpy(this->address, broker_address ? broker_address : "", sizeof(this->address) - 1);
    port = broker_port;
    setBrokerCredentials(broker_user, broker_pass);

    /* Local MongoDB parameters */
    memset(this->database_name, '\0', sizeof(this->database_name));
    memset(this->projectServer, '\0', sizeof(this->projectServer));
    strncpy(this->database_name, database_name ? database_name : "",
            sizeof(this->database_name) - 1);
    strncpy(this->projectServer, projectServer ? projectServer : "",
            sizeof(this->projectServer) - 1);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MongoDB::Loom_MongoDB(Manager &man, NetworkComponent &internet_client)
    : MQTTComponent("MongoDB", internet_client), manInst(&man) {
    setClientID(man.get_serial_num());
    memset(database_name, '\0', sizeof(database_name));
    memset(projectServer, '\0', sizeof(projectServer));
    moduleInitialized = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::buildTopic() {
    return buildTopic(manInst->get_device_name(), manInst->get_instance_num());
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::buildTopic(const char *deviceName, int instance) {
    if (!loomMQTT::validTopic(deviceName, Manager::DEVICE_NAME_SIZE, true) ||
        !loomMQTT::validTopic(database_name, sizeof(database_name), true) ||
        (projectServer[0] != '\0' &&
         !loomMQTT::validTopic(projectServer, sizeof(projectServer), true))) {
        ERROR(F("Invalid device identity; refusing to build an MQTT topic."));
        return false;
    }
    // Preserve the established optional project/database/device topic format.
    const int written = projectServer[0] != '\0'
                            ? snprintf_P(topic, sizeof(topic), PSTR("%s/%s/%s%i"), projectServer,
                                         database_name, deviceName, instance)
                            : snprintf_P(topic, sizeof(topic), PSTR("%s/%s%i"), database_name,
                                         deviceName, instance);
    return written > 0 && static_cast<size_t>(written) < sizeof(topic);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::validateBatchRecord(Stream &source) {
    // A queued hub file can contain many nodes. Read only id.name/id.instance into small fixed
    // documents; never build a second full sensor packet or use the last received node's name.
    StaticJsonDocument<JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2)> filter;
    filter.to<JsonObject>();
    if (batchUsesPacketIdentity) {
        filter["id"]["name"] = true;
        filter["id"]["instance"] = true;
    }
    StaticJsonDocument<256> identity;
    const DeserializationError error =
        deserializeJson(identity, source, DeserializationOption::Filter(filter));
    if (error || identity.overflowed()) {
        ERROR(F("Batch JSON is malformed; retaining the file instead of discarding its records."));
        return false;
    }
    if (!batchUsesPacketIdentity) {
        return true; // Validate syntax without requiring identity in legacy custom batches.
    }
    const JsonString name = identity["id"]["name"].as<JsonString>();
    if (name.isNull() ||
        memchr(name.c_str(), '\0', name.size()) != nullptr ||
        !identity["id"]["instance"].is<int>() || identity["id"]["instance"].as<int>() < 0) {
        ERROR(F("Batch identity is malformed; retaining the file instead of misrouting it."));
        return false;
    }
    return buildTopic(name.c_str(), identity["id"]["instance"].as<int>());
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::publish() {
    FUNCTION_START(this);
    if (!manInst->isPacketValid()) {
        ERROR(F("Refusing to publish an empty or overflowed JSON packet."));
        return false;
    }
    if (!moduleInitialized) {
        WARNING(F("Module not initialized! If using credentials from SD make sure they are loaded "
                  "first."));
        return false;
    }
    if (!buildTopic()) {
        return false;
    }
    if (!connectToBroker()) {
        return false;
    }
    LOGF("MQTT publish attempt: topic=%s bytes=%u", topic,
         static_cast<unsigned int>(measureJson(manInst->getDocument())));
    const bool published = publishDocument(topic, manInst->getDocument());
    if (published) {
        LOG(F("MQTT broker acknowledged the packet; database persistence is not confirmed."));
    }
    FUNCTION_END;
    return published;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::publishMetadata(char *metadata) {
    FUNCTION_START(this);
    if (metadata == nullptr) {
        ERROR(F("Cannot publish null metadata."));
        return false;
    }
    if (!moduleInitialized) {
        WARNING(F("Module not initialized! If using credentials from SD make sure they are loaded "
                  "first."));
        return false;
    }
    if (!buildTopic()) {
        return false;
    }
    if (!connectToBroker()) {
        return false;
    }
    LOG(F("Attempting to publish metadata!"));
    FUNCTION_END;
    return publishMessage(topic, metadata);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::publishMetadata(const JsonDocument &metadata) {
    FUNCTION_START(this);
    const char *name = metadata["id"]["name"].as<const char *>();
    if (!loomJsonIsComplete(metadata) ||
        !loomHeartbeat::validIdentity(metadata["id"].as<JsonObjectConst>()) || name == nullptr ||
        strcmp(name, manInst->get_device_name()) != 0 || !metadata["id"]["instance"].is<int>() ||
        metadata["id"]["instance"].as<int>() != manInst->get_instance_num() || !moduleInitialized ||
        !buildTopic() || !connectToBroker()) {
        return false;
    }
    const bool published = publishDocument(topic, metadata);
    FUNCTION_END;
    return published;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::publishHeartbeat(const JsonDocument &heartbeat) {
    return loomHeartbeat::isStatusPayload(heartbeat.as<JsonObjectConst>()) &&
           publishMetadata(heartbeat);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::publish(Loom_BatchSD &batchSD) {
    FUNCTION_START(this);

    const float batteryVoltage = Loom_Analog::getBatteryVoltage();
    if (!isfinite(batteryVoltage) || batteryVoltage <= 0.0f) {
        WARNING(F("Battery reading unavailable; retaining the batch instead of starting LTE."));
        return false;
    }
    if (batteryVoltage < 3.4f) {
        const uint16_t mv = (uint16_t)(batteryVoltage * 1000.0f + 0.5f);
        WARNINGF("Battery voltage %u.%03uV is below the 3.40V transmission threshold.",
                 (unsigned int)(mv / 1000), (unsigned int)(mv % 1000));
        return false;
    }

    if (!moduleInitialized) {
        WARNING(F("Module not initialized! If using credentials from SD make sure they are loaded "
                  "first."));
        return false;
    }
    if (!batchSD.shouldPublish()) {
        LOGF("Batch not ready to publish: %i/%i", batchSD.getCurrentBatch(),
             batchSD.getBatchSize());
        return false;
    }

    if (!buildTopic()) {
        return false;
    }
    int packetNumber = 0;
    /* Attempt to connect to the broker */
    if (!connectToBroker()) {
        return false;
    }

    /* Get the file containing our batch of data */
    // Retain a small independent SdFat handle. Ordinary timestamped LOG calls also append
    // to SD and must not replace or close the batch reader while it is being streamed.
    File fileOutput = batchSD.openBatch();
    if (!fileOutput) {
        ERROR(F("Unable to open the BatchSD file."));
        return false;
    }

    bool allDataSuccess = true;

    // MQTT requires the payload length before the body. Scan each line once for its length,
    // seek back, and then stream it in small chunks instead of reserving 2 KB on the stack.
    while (true) {
        uint32_t lineStart = 0;
        size_t lineLength = 0;
        bool terminated = false;
        const loomSD::RecordResult result =
            loomSD::nextRecord(fileOutput, lineStart, lineLength, MAX_JSON_SIZE, &terminated);
        if (result == loomSD::RecordResult::End) {
            break;
        }
        if (result != loomSD::RecordResult::Ready || !terminated ||
            lineLength > UINT32_MAX - lineStart) {
            ERROR(result == loomSD::RecordResult::ReadError
                      ? F("SD read failed during batch scan; retaining the batch.")
                      : F("Batch packet is oversized or unterminated; retaining the batch."));
            allDataSuccess = false;
            break;
        }
        const uint32_t lineEnd = lineStart + static_cast<uint32_t>(lineLength);

        ++packetNumber;
        LOGF("Publishing Packet %i of %i", packetNumber, batchSD.getCurrentBatch());
        if (!fileOutput.seekSet(lineStart)) {
            ERROR(F("SD seek failed before batch publish; retaining the batch."));
            allDataSuccess = false;
            break;
        }
        if (!validateBatchRecord(fileOutput) ||
            !loomSD::finishJsonRecord(fileOutput, lineEnd) ||
            !fileOutput.seekSet(lineStart)) {
            ERROR(F("Batch record could not be validated within its SD line; retaining the file."));
            allDataSuccess = false;
            break;
        }
        // File + byte offset + topic link this attempt to the exact original SD record without
        // changing the JSON schema. A broker acknowledgement does not prove a database insert.
        LOGF("MQTT batch attempt: file=%s offset=%lu bytes=%u topic=%s", batchSD.getBatchFilename(),
             static_cast<unsigned long>(lineStart), static_cast<unsigned int>(lineLength), topic);
        if (!publishStream(topic, fileOutput, lineLength)) {
            WARNINGF("Failed to publish packet #%i; retaining the whole batch for retry.",
                     packetNumber);
            allDataSuccess = false;
            break;
        }
        LOGF("MQTT broker acknowledged packet #%i; database persistence is not confirmed.",
             packetNumber);
        // Move past the successfully sent body. A failed publish exits above and preserves the
        // entire file; the next attempt starts from its first record, not a partial remainder.
        if (!fileOutput.seekSet(lineEnd)) {
            ERROR(F("SD seek failed after batch publish; retaining the batch."));
            allDataSuccess = false;
            break;
        }
        // publishStream leaves the cursor just before the delimiter. The next outer
        // iteration consumes CR, LF, or both.
        delay(500);
    }
    const bool readSucceeded = fileOutput.getError() == 0;
    const bool closed = fileOutput.close();
    if (!readSucceeded || !closed) {
        ERROR(F("Batch reader failed or did not close; retaining the batch."));
        return false;
    }

    if (!allDataSuccess) {
        return false;
    }

    if (packetNumber == 0) {
        ERROR(F("BatchSD counter is ready, but the batch file contains no records."));
        return false;
    }

    if (packetNumber != batchSD.getCurrentBatch()) {
        ERRORF("BatchSD record/count mismatch (%i file records, %i counted); retaining "
               "the file.",
               packetNumber, batchSD.getCurrentBatch());
        return false;
    }

    if (!batchSD.markPublished()) {
        ERROR(F("Batch was sent, but its SD file could not be cleared; it will be retried."));
        return false;
    }
    LOG(F("MQTT broker acknowledged all batch records; upload queue cleared."));
    FUNCTION_END;
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MongoDB::loadConfigFromJSON(char *json) {
    FUNCTION_START(this);

    if (json == nullptr) {
        ERROR(F("Cannot load MQTT credentials from a null buffer."));
        moduleInitialized = false;
        return;
    }

    // Doc to store the JSON data from the SD card in
    // Mutable input enables ArduinoJson's zero-copy mode; only the object nodes need document RAM.
    StaticJsonDocument<JSON_OBJECT_SIZE(6)> doc;
    DeserializationError deserialError = deserializeJson(doc, json);

    // Check if an error occurred and if so print it
    if (deserialError != DeserializationError::Ok) {
        ERRORF("There was an error reading the MQTT credentials from SD: %s",
               deserialError.c_str());
        free(json);
        moduleInitialized = false;
        return;
    }

    // Wrong JSON types used to become null pointers passed to strncpy(), restarting a device
    // during SD setup. Validate before copying; retain no borrowed pointers after freeing json.
    const char *broker = doc["broker"].as<const char *>();
    const char *database = doc["database"].as<const char *>();
    const char *project = doc["project"] | "";
    const bool optionalStringsValid =
        (doc["project"].isNull() || doc["project"].is<const char *>()) &&
        (doc["username"].isNull() || doc["username"].is<const char *>()) &&
        (doc["password"].isNull() || doc["password"].is<const char *>());
    const int brokerPort = doc["port"].as<int>();
    if (broker == nullptr || broker[0] == '\0' || database == nullptr || database[0] == '\0' ||
        !optionalStringsValid || !doc["port"].is<int>() || brokerPort < 1 || brokerPort > 65535 ||
        strlen(broker) >= sizeof(address) || strlen(database) >= sizeof(database_name) ||
        strlen(project) >= sizeof(projectServer) ||
        !loomMQTT::validTopic(database, sizeof(database_name), true) ||
        (project[0] != '\0' && !loomMQTT::validTopic(project, sizeof(projectServer), true)) ||
        !configStringIsComplete(doc["broker"]) || !configStringIsComplete(doc["database"]) ||
        !configStringIsComplete(doc["project"]) || !configStringIsComplete(doc["username"]) ||
        !configStringIsComplete(doc["password"])) {
        ERROR(F("Invalid MQTT configuration: check broker/database strings, lengths, and port."));
        moduleInitialized = false;
        free(json);
        return;
    }

    // Clear the strings and set port = 0
    memset(address, '\0', sizeof(address));
    memset(database_name, '\0', sizeof(database_name));
    memset(projectServer, '\0', sizeof(projectServer));
    port = 0;

    strncpy(address, broker, sizeof(address) - 1);
    strncpy(database_name, database, sizeof(database_name) - 1);

    setBrokerCredentials(doc["username"] | "", doc["password"] | "");

    strncpy(projectServer, project, sizeof(projectServer) - 1);
    port = brokerPort;

    moduleInitialized = true;
    free(json);
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
