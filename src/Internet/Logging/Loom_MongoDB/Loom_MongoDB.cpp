#include "Loom_MongoDB.h"
#include "../../../Hardware/Loom_BatchSD/Loom_BatchSD.h"
#include "../../../Sensors/Loom_Analog/Loom_Analog.h"
#include "Logger.h"
#include "Utilities/Loom_SDUtils.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MongoDB::Loom_MongoDB(Manager &man, NetworkComponent &internet_client,
                           const char *broker_address, int broker_port, const char *database_name,
                           const char *broker_user, const char *broker_pass,
                           const char *projectServer)
    : MQTTComponent("MongoDB", internet_client), manInst(&man) {
    /* MQTT Connection parameters */
    strncpy(this->address, broker_address, sizeof(this->address) - 1);
    port = broker_port;
    setBrokerCredentials(broker_user, broker_pass);

    /* Local MongoDB parameters */
    memset(this->database_name, '\0', sizeof(this->database_name));
    memset(this->projectServer, '\0', sizeof(this->projectServer));
    strncpy(this->database_name, database_name, sizeof(this->database_name) - 1);
    strncpy(this->projectServer, projectServer, sizeof(this->projectServer) - 1);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MongoDB::Loom_MongoDB(Manager &man, NetworkComponent &internet_client)
    : MQTTComponent("MongoDB", internet_client), manInst(&man) {
    memset(database_name, '\0', sizeof(database_name));
    memset(projectServer, '\0', sizeof(projectServer));
    moduleInitialized = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MongoDB::buildTopic() {
    // Keep the established optional project/database/device topic format.
    if (projectServer[0] != '\0') {
        snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s/%s%i"), projectServer, database_name,
                   manInst->get_device_name(), manInst->get_instance_num());
    } else {
        snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s%i"), database_name,
                   manInst->get_device_name(), manInst->get_instance_num());
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::publish() {
    FUNCTION_START;
    if (!manInst->isPacketValid()) {
        ERROR(F("Refusing to publish an empty or overflowed JSON packet."));
        return false;
    }
    if (!moduleInitialized) {
        WARNING(F("Module not initialized! If using credentials from SD make sure they are loaded "
                  "first."));
        return false;
    }
    buildTopic();
    if (!connectToBroker()) {
        return false;
    }
    FUNCTION_END;
    return publishDocument(topic, manInst->getDocument());
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::publishMetadata(char *metadata) {
    FUNCTION_START;
    if (metadata == nullptr) {
        ERROR(F("Cannot publish null metadata."));
        return false;
    }
    if (!moduleInitialized) {
        WARNING(F("Module not initialized! If using credentials from SD make sure they are loaded "
                  "first."));
        return false;
    }
    buildTopic();
    if (!connectToBroker()) {
        return false;
    }
    LOG(F("Attempting to publish metadata!"));
    FUNCTION_END;
    return publishMessage(topic, metadata);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::publish(Loom_BatchSD &batchSD) {
    FUNCTION_START;

    const float batteryVoltage = Loom_Analog::getBatteryVoltage();
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

    buildTopic();
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
        const loomSD::RecordResult result =
            loomSD::nextRecord(fileOutput, lineStart, lineLength, MAX_JSON_SIZE);
        if (result == loomSD::RecordResult::End) {
            break;
        }
        if (result != loomSD::RecordResult::Ready) {
            ERROR(result == loomSD::RecordResult::ReadError
                      ? F("SD read failed during batch scan; retaining the batch.")
                      : F("Batch packet exceeds MAX_JSON_SIZE; retaining the batch."));
            allDataSuccess = false;
            break;
        }

        ++packetNumber;
        LOGF("Publishing Packet %i of %i", packetNumber, batchSD.getCurrentBatch());
        if (!fileOutput.seekSet(lineStart)) {
            ERROR(F("SD seek failed before batch publish; retaining the batch."));
            allDataSuccess = false;
            break;
        }
        if (!publishStream(topic, fileOutput, lineLength)) {
            WARNINGF("Failed to publish packet #%i", packetNumber);
            allDataSuccess = false;
        }
        // On a partial network write, restore the cursor to the end of this record so its
        // remainder cannot be mistaken for a new packet.
        if (!fileOutput.seekSet(lineStart + lineLength)) {
            ERROR(F("SD seek failed after batch publish; retaining the batch."));
            allDataSuccess = false;
            break;
        }
        // publishStream leaves the cursor just before the delimiter. The next outer
        // iteration consumes CR, LF, or both.
        delay(500);
    }
    fileOutput.close();

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
    LOG(F("Data has been successfully sent!"));
    FUNCTION_END;
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MongoDB::loadConfigFromJSON(char *json) {
    FUNCTION_START;

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

    // Clear the strings and set port = 0
    memset(address, '\0', sizeof(address));
    memset(database_name, '\0', sizeof(database_name));
    memset(projectServer, '\0', sizeof(projectServer));
    port = 0;

    /* We should check if any parameter is null */
    if (!doc["broker"].isNull()) {
        strncpy(address, doc["broker"].as<const char *>(), sizeof(address) - 1);
    }

    if (!doc["database"].isNull()) {
        strncpy(database_name, doc["database"].as<const char *>(), sizeof(database_name) - 1);
    }

    setBrokerCredentials(doc["username"] | "", doc["password"] | "");

    if (!doc["project"].isNull()) {
        strncpy(projectServer, doc["project"].as<const char *>(), sizeof(projectServer) - 1);
    }

    if (!doc["port"].isNull()) {
        port = doc["port"].as<int>();
    }

    moduleInitialized = true;
    free(json);
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
