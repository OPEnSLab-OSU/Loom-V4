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
    strncpy(this->address, broker_address, 100);
    port = broker_port;
    strncpy(this->username, broker_user, 100);
    strncpy(this->password, broker_pass, 100);

    /* Local MongoDB parameters */
    strncpy(this->database_name, database_name, 100);
    strncpy(this->projectServer, projectServer, 100);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MongoDB::Loom_MongoDB(Manager &man, NetworkComponent &internet_client)
    : MQTTComponent("MongoDB", internet_client), manInst(&man) {
    moduleInitialized = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_MongoDB::publish() {
    FUNCTION_START;

    char jsonString[MAX_JSON_SIZE];
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
        manInst->getJSONString(jsonString);
        if (!publishMessage(topic, jsonString)) {
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

    if (moduleInitialized) {

        char jsonString[MAX_JSON_SIZE];
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
        /* Attempt to publish the data to the given topic */
        if (!publishMessage(topic, metadata)) {
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
bool Loom_MongoDB::publish(Loom_BatchSD &batchSD) {
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    if (Loom_Analog::getBatteryVoltage() < 3.4) {
        WARNING(F("Module not initialized! Battery doesn't have enough power."));
        FUNCTION_END;
        return false;
    }

    char line[MAX_JSON_SIZE] = {};
    int packetNumber = 0;
    int index = 0;
    bool lineOverflow = false;
    if (moduleInitialized) {
        // TIMER_DISABLE;
        if (batchSD.shouldPublish()) {

            if (strlen(projectServer) > 0)
                // Formulate a topic to publish on with the format
                // "ProjectName/DatabaseName/DeviceNameInstanceNumber" eg.
                // WeatherChimes/Chimes/Chime1
                snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s%i"), database_name,
                           manInst->get_device_name(), manInst->get_instance_num());
            else
                // Formulate a topic to publish on with the format
                // "DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chime1
                snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s%i"), database_name,
                           manInst->get_device_name(), manInst->get_instance_num());

            /* Attempt to connect to the broker */
            if (!connectToBroker())
                return false;

            /* Get the file containing our batch of data */
            File fileOutput = batchSD.getBatch();

            bool allDataSuccess = true;
            auto publishCurrentLine = [&]() {
                packetNumber++;

                if (lineOverflow) {
                    snprintf_P(output, OUTPUT_SIZE,
                               PSTR("Dropped oversized packet #%i (len >= %i bytes)"),
                               packetNumber, MAX_JSON_SIZE);
                    WARNING(output);
                    allDataSuccess = false;
                    lineOverflow = false;
                    index = 0;
                    line[0] = '\0';
                    return;
                }

                if (index <= 0) {
                    line[0] = '\0';
                    return;
                }

                // Ensure null-termination before publish
                line[index] = '\0';

                snprintf_P(output, OUTPUT_SIZE, PSTR("Publishing Packet %i of %i with len=%d"),
                           packetNumber, batchSD.getBatchSize(), index);
                LOG(output);

                if (!publishMessage(topic, line)) {
                    snprintf_P(output, OUTPUT_SIZE, PSTR("Failed to publish packet #%i"),
                               packetNumber);
                    WARNING(output);
                    allDataSuccess = false;
                }

                delay(500);
                index = 0;
                line[0] = '\0';
            };

            /* Utilize a stream so it doesn't matter how much data we have as its read in one by one
             */
            while (fileOutput.available()) {
                int readValue = fileOutput.read();
                if (readValue < 0) {
                    WARNING(F("Failed to read from batch file."));
                    allDataSuccess = false;
                    break;
                }
                char c = (char)readValue;

                /* Attempt to reconnect if connection has been stopped during publishMessage
                 * The previous packet that was lost due a stopped connected will not be
                 * retransmitted.
                 */
                if (!isConnected()) {
                    connectToBroker();
                    if (!isConnected()) {
                        WARNING(F("Connection lost during batch publish."));
                        allDataSuccess = false;
                        break;
                    }
                }

                // Handle both CR and LF line endings.
                if (c == '\r' || c == '\n') {
                    // Ignore empty lines and LF from CRLF pairs.
                    if (index > 0 || lineOverflow) {
                        publishCurrentLine();
                    }
                    continue;
                }

                // Add data while leaving room for null terminator.
                if (index < (MAX_JSON_SIZE - 1)) {
                    line[index] = c;
                    index++;
                } else {
                    lineOverflow = true;
                }
            }

            // Flush trailing data if file does not end in CR/LF.
            if (index > 0 || lineOverflow) {
                publishCurrentLine();
            }
            fileOutput.close();

            // Check if we actually sent all the data successfully
            if (allDataSuccess)
                LOG(F("Data has been successfully sent!"));
            else {
                WARNING(F("1 or more packets failed to send!"));
                FUNCTION_END;
                return false;
            }

        } else {
            snprintf_P(output, OUTPUT_SIZE, PSTR("Batch not ready to publish: %i/%i"),
                       batchSD.getCurrentBatch(), batchSD.getBatchSize());
            LOG(output);
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
    char output[OUTPUT_SIZE];
    char topic[MAX_TOPIC_LENGTH];

    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<300> doc;
    DeserializationError deserialError = deserializeJson(doc, (const char *)json);

    // Check if an error occurred and if so print it
    if (deserialError != DeserializationError::Ok) {
        snprintf_P(output, OUTPUT_SIZE,
                   PSTR("There was an error reading the MQTT credentials from SD: %s"),
                   deserialError.c_str());
        ERROR(output);
    }

    // Clear the strings and set port = 0
    memset(address, '\0', 100);
    memset(database_name, '\0', 100);
    memset(username, '\0', 100);
    memset(password, '\0', 100);
    port = 0;

    /* We should check if any parameter is null */
    if (!doc["broker"].isNull())
        strncpy(address, doc["broker"].as<const char *>(), 100);

    if (!doc["database"].isNull())
        strncpy(database_name, doc["database"].as<const char *>(), 100);

    if (!doc["username"].isNull())
        strncpy(username, doc["username"].as<const char *>(), 100);

    if (!doc["password"].isNull())
        strncpy(password, doc["password"].as<const char *>(), 100);

    if (!doc["project"].isNull())
        strncpy(projectServer, doc["project"].as<const char *>(), 100);

    if (!doc["port"].isNull())
        port = doc["port"].as<int>();

    if (strlen(projectServer) > 0) {
        // Formulate a topic to publish on with the format
        // "ProjectName/DatabaseName/DeviceNameInstanceNumber" eg. WeatherChimes/Chimes/Chime1
        snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s/%s%i"), projectServer, database_name,
                   manInst->get_device_name(), manInst->get_instance_num());
    } else {
        // Formulate a topic to publish on with the format "DatabaseName/DeviceNameInstanceNumber"
        // eg. WeatherChimes/Chime1
        snprintf_P(topic, MAX_TOPIC_LENGTH, PSTR("%s/%s%i"), database_name,
                   manInst->get_device_name(), manInst->get_instance_num());
    }

    moduleInitialized = true;
    free(json);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
