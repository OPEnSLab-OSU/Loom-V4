#include "Loom_MongoDB.h"
#include "../../../Sensors/Loom_Analog/Loom_Analog.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MongoDB::Loom_MongoDB(Manager &man, NetworkComponent &internet_client)
    : MQTTComponent("MongoDB", internet_client), manInst(&man) {
    memset(database_name, '\0', sizeof(database_name));
    memset(projectServer, '\0', sizeof(projectServer));
    moduleInitialized = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::publish() {
    FUNCTION_START;

    if (moduleInitialized) {

        // TIMER_DISABLE;

        if (strlen(projectServer) > 0)
            // Formulate a topic to publish on with the format
            // "ProjectName/DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chimes/Chime1
            snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s/%s%i"), projectServer, database_name,
                       manInst->get_device_name(), manInst->get_instance_num());
        else
            // Formulate a topic to publish on with the format
            // "DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chime1
            snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s%i"), database_name,
                       manInst->get_device_name(), manInst->get_instance_num());

        /* Attempt to connect to the broker if it fails we should just return */
        if (!connectToBroker()) {
            FUNCTION_END;
            return false;
        }

        /* Attempt to publish the data to the given topic */
        if (!publishDocument(topic, manInst->getDocument())) {
            FUNCTION_END;
            return false;
        }
    } else {
        WARNING(F("Module not initialized! If using credentials from SD make sure they are loaded "
                  "first."));
        FUNCTION_END;
        return false;
    }
    FUNCTION_END;
    // TIMER_ENABLE;
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::publishMetadata(char *metadata) {
    FUNCTION_START;

    if (metadata == nullptr) {
        ERROR(F("Cannot publish null metadata."));
        FUNCTION_END;
        return false;
    }

    if (moduleInitialized) {

        // char jsonString[MAX_JSON_SIZE]; // Metadata is already supplied as the method argument.
        // TIMER_DISABLE;

        if (strlen(projectServer) > 0)
            // Formulate a topic to publish on with the format
            // "ProjectName/DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chimes/Chime1
            snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s/%s%i"), projectServer, database_name,
                       manInst->get_device_name(), manInst->get_instance_num());
        else
            // Formulate a topic to publish on with the format
            // "DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chime1
            snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s%i"), database_name,
                       manInst->get_device_name(), manInst->get_instance_num());

        /* Attempt to connect to the broker if it fails we should just return */
        if (!connectToBroker()) {
            FUNCTION_END;
            return false;
        }

        LOG(F("Attempting to publish metadata!"));
        const bool published = publishMessage(topic, metadata);
        FUNCTION_END;
        return published;
    } else {
        WARNING(F("Module not initialized! If using credentials from SD make sure they are loaded "
                  "first."));
        FUNCTION_END;
        return false;
    }
    FUNCTION_END;
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::publish(Loom_BatchSD &batchSD) {
    FUNCTION_START;

    const float batteryVoltage = Loom_Analog::getBatteryVoltage();
    if (batteryVoltage < 3.4f) {
        const uint16_t mv = (uint16_t)(batteryVoltage * 1000.0f + 0.5f);
        WARNINGF("Battery voltage %u.%03uV is below the 3.40V transmission threshold.",
                 (unsigned int)(mv / 1000), (unsigned int)(mv % 1000));
        FUNCTION_END;
        return false;
    }

    int packetNumber = 0;
    if (moduleInitialized) {
        // TIMER_DISABLE;
        if (batchSD.shouldPublish()) {

            if (strlen(projectServer) > 0)
                // Formulate a topic to publish on with the format
                // "ProjectName/DatabaseName/DeviceNameInstanceNumber" eg.
                // WeatherChimes/Chimes/Chime1
                snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s/%s%i"), projectServer,
                           database_name, manInst->get_device_name(), manInst->get_instance_num());
            else
                // Formulate a topic to publish on with the format
                // "DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chime1
                snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s%i"), database_name,
                           manInst->get_device_name(), manInst->get_instance_num());

            /* Attempt to connect to the broker */
            if (!connectToBroker())
                return false;

            /* Get the file containing our batch of data */
            // Retain a small independent SdFat handle. Ordinary timestamped LOG calls also append
            // to SD and must not replace or close the batch reader while it is being streamed.
            File fileOutput = batchSD.openBatch();
            if (!fileOutput) {
                ERROR(F("Unable to open the BatchSD file."));
                FUNCTION_END;
                return false;
            }

            bool allDataSuccess = true;

            // MQTT requires the payload length before the body. Scan each line once for its length,
            // seek back, and then stream it in small chunks instead of reserving 2 KB on the stack.
            while (fileOutput.available()) {
                int value = fileOutput.read();
                if (value == '\r' || value == '\n')
                    continue;

                const uint32_t lineStart = fileOutput.curPosition() - 1;
                size_t lineLength = 1;
                bool lineTooLong = false;
                while (fileOutput.available()) {
                    value = fileOutput.read();
                    if (value == '\r' || value == '\n')
                        break;
                    ++lineLength;
                    if (lineLength >= MAX_JSON_SIZE)
                        lineTooLong = true;
                }

                ++packetNumber;
                if (lineTooLong) {
                    ERROR(F("Batch packet exceeds MAX_JSON_SIZE and was skipped."));
                    allDataSuccess = false;
                    continue;
                }

                LOGF("Publishing Packet %i of %i", packetNumber, batchSD.getCurrentBatch());
                if (!fileOutput.seekSet(lineStart) ||
                    !publishStream(topic, fileOutput, lineLength)) {
                    WARNINGF("Failed to publish packet #%i", packetNumber);
                    allDataSuccess = false;
                }
                // On a partial network write, restore the cursor to the end of this record so its
                // remainder cannot be mistaken for a new packet.
                fileOutput.seekSet(lineStart + lineLength);
                // publishStream leaves the cursor just before the delimiter. The next outer
                // iteration consumes CR, LF, or both.
                delay(500);
            }
            fileOutput.close();

            if (packetNumber == 0) {
                ERROR(F("BatchSD counter is ready, but the batch file contains no records."));
                FUNCTION_END;
                return false;
            }

            if (packetNumber != batchSD.getCurrentBatch()) {
                ERRORF("BatchSD record/count mismatch (%i file records, %i counted); retaining "
                       "the file.",
                       packetNumber, batchSD.getCurrentBatch());
                FUNCTION_END;
                return false;
            }

            // Check if we actually sent all the data successfully
            if (allDataSuccess) {
                if (!batchSD.markPublished()) {
                    ERROR(F("Batch was sent, but its SD file could not be cleared; it will be "
                            "retried."));
                    FUNCTION_END;
                    return false;
                }
                LOG(F("Data has been successfully sent!"));
            } else {
                WARNING(F("1 or more packets failed to send!"));
                FUNCTION_END;
                return false;
            }

        } else {
            LOGF("Batch not ready to publish: %i/%i", batchSD.getCurrentBatch(),
                 batchSD.getBatchSize());
            FUNCTION_END;
            return false;
        }
    } else {
        WARNING(F("Module not initialized! If using credentials from SD make sure they are loaded "
                  "first."));
        FUNCTION_END;
        return false;
    }
    FUNCTION_END;
    // TIMER_ENABLE;
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MongoDB::loadConfigFromJSON(char *json) {
    FUNCTION_START;

    if (json == nullptr) {
        ERROR(F("Cannot load MQTT credentials from a null buffer."));
        moduleInitialized = false;
        FUNCTION_END;
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
        FUNCTION_END;
        return;
    }

    // Clear the strings and set port = 0
    memset(address, '\0', sizeof(address));
    memset(database_name, '\0', sizeof(database_name));
    memset(projectServer, '\0', sizeof(projectServer));
    port = 0;

    /* We should check if any parameter is null */
    if (!doc["broker"].isNull())
        strncpy(address, doc["broker"].as<const char *>(), sizeof(address) - 1);

    if (!doc["database"].isNull())
        strncpy(database_name, doc["database"].as<const char *>(), sizeof(database_name) - 1);

    setBrokerCredentials(doc["username"] | "", doc["password"] | "");

    if (!doc["project"].isNull())
        strncpy(projectServer, doc["project"].as<const char *>(), sizeof(projectServer) - 1);

    if (!doc["port"].isNull())
        port = doc["port"].as<int>();

    moduleInitialized = true;
    free(json);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
