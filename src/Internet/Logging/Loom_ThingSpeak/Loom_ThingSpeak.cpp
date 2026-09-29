#include "Loom_ThingSpeak.h"
#include "Logger.h"

namespace {
bool appendText(char *destination, size_t capacity, const char *suffix) {
    if (destination == nullptr || suffix == nullptr || capacity == 0) {
        return false;
    }

    const size_t used = strnlen(destination, capacity);
    const size_t suffixLength = strlen(suffix);
    if (used >= capacity || suffixLength > capacity - used - 1) {
        return false;
    }

    memcpy(destination + used, suffix, suffixLength + 1);
    return true;
}
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ThingSpeak::Loom_ThingSpeak(Manager &man, NetworkComponent &internet_client, int channelID,
                                 const char *clientID, const char *broker_user,
                                 const char *broker_pass)
    : MQTTComponent("ThingSpeak", internet_client), manager(&man) {
    /* Thing speak server parameters */
    strncpy(this->address, "mqtt3.thingspeak.com", 100);
    port = 1883;

    /* ThingSpeak provided connection details */
    setClientID(clientID);
    setBrokerCredentials(broker_user, broker_pass);
    this->channelID = channelID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ThingSpeak::Loom_ThingSpeak(Manager &man, NetworkComponent &internet_client)
    : MQTTComponent("ThingSpeak", internet_client), manager(&man) {
    moduleInitialized = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_ThingSpeak::publish() {
    FUNCTION_START;
    if (!moduleInitialized) {
        WARNING(F("Module not initialized! If using credentials from SD make sure they are loaded "
                  "first."));
        return false;
    }
    if (!connectToBroker()) {
        return false;
    }

    char message[MESSAGE_SIZE];
    char topic[MAX_TOPIC_LENGTH];
    if (!formatMessage(topic, message)) {
        ERROR(F("ThingSpeak message exceeded its fixed buffer."));
        return false;
    }
    FUNCTION_END;
    return publishMessage(topic, message, false, 0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_ThingSpeak::publish(Loom_BatchSD &batchSD) {
    (void)batchSD;
    ERROR(F("ThingSpeak batch replay is not supported by the callback-based field API."));
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ThingSpeak::addFunction(int fieldNumber, FloatReturnFuncDefs readValue) {
    if (readValue == nullptr) {
        ERROR(F("Cannot add a null ThingSpeak field function."));
        return;
    }
    if (fieldsWithoutParameters.size() + fieldsWithParameters.size() >= MAX_FIELDS) {
        WARNING(F("ThingSpeak supports at most eight fields; field was not retained."));
        return;
    }
    fieldsWithoutParameters.push_back({fieldNumber, readValue});
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ThingSpeak::addFunction(int fieldNumber, FloatReturnFuncDefsWithParam readValue,
                                  int parameter) {
    if (readValue == nullptr) {
        ERROR(F("Cannot add a null ThingSpeak field function."));
        return;
    }
    if (fieldsWithoutParameters.size() + fieldsWithParameters.size() >= MAX_FIELDS) {
        WARNING(F("ThingSpeak supports at most eight fields; field was not retained."));
        return;
    }
    fieldsWithParameters.push_back({fieldNumber, readValue, parameter});
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_ThingSpeak::formatMessage(char topic[MAX_TOPIC_LENGTH], char message[MESSAGE_SIZE]) {
    char tempBuffer[100] = {};

    memset(topic, '\0', MAX_TOPIC_LENGTH);
    memset(message, '\0', MESSAGE_SIZE);

    /* Format the topic to publish to many fields at once */
    snprintf(topic, MAX_TOPIC_LENGTH, "channels/%i/publish", channelID);

    // Check if combined size of both function lists is more than 8
    if (fieldsWithoutParameters.size() + fieldsWithParameters.size() > MAX_FIELDS) {
        WARNING(F("There have been more than 8 fields added. ThingSpeak only supports up to 8 "
                  "fields so any fields after the initial 8 will be ignored"));
    }

    size_t totalAdded = 0;
    for (const FieldFunction &field : fieldsWithoutParameters) {
        if (totalAdded >= MAX_FIELDS) {
            break;
        }
        snprintf(tempBuffer, sizeof(tempBuffer), "field%i=%f&", field.fieldNumber,
                 field.readValue());
        if (!appendText(message, MESSAGE_SIZE, tempBuffer)) {
            return false;
        }
        ++totalAdded;
    }

    for (const ParameterizedFieldFunction &field : fieldsWithParameters) {
        if (totalAdded >= MAX_FIELDS) {
            break;
        }
        snprintf(tempBuffer, sizeof(tempBuffer), "field%i=%f&", field.fieldNumber,
                 field.readValue(field.parameter));
        if (!appendText(message, MESSAGE_SIZE, tempBuffer)) {
            return false;
        }
        ++totalAdded;
    }

    /* Check if we have a timestamp property; if so, add a "created_at" parameter to the
     * message that we are publishing */
    if (!manager->getDocument()["timestamp"].isNull()) {
        const char *localTime =
            manager->getDocument()["timestamp"]["time_local"].as<const char *>();
        if (localTime != nullptr) {
            snprintf(tempBuffer, sizeof(tempBuffer), "created_at=%s&", localTime);
            if (!appendText(message, MESSAGE_SIZE, tempBuffer)) {
                return false;
            }
        }
    }

    /* Finally end the message with the status */
    return appendText(message, MESSAGE_SIZE, "status=MQTTPUBLISH");
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ThingSpeak::loadConfigFromJSON(char *json) {
    FUNCTION_START;

    if (json == nullptr) {
        ERROR(F("Cannot load ThingSpeak credentials from a null buffer."));
        moduleInitialized = false;
        return;
    }

    // Mutable input enables zero-copy strings; only the four object slots occupy document RAM.
    StaticJsonDocument<JSON_OBJECT_SIZE(4)> doc;
    DeserializationError deserialError = deserializeJson(doc, json);

    // Check if an error occurred and if so print it
    if (deserialError != DeserializationError::Ok) {
        ERRORF("There was an error reading the MQTT credentials from SD: %s",
               deserialError.c_str());
        free(json);
        moduleInitialized = false;
        return;
    }

    if (!doc["channelID"].isNull()) {
        channelID = doc["channelID"].as<int>();
    }

    if (!doc["clientID"].isNull()) {
        setClientID(doc["clientID"].as<const char *>());
    }

    setBrokerCredentials(doc["username"] | "", doc["password"] | "");

    moduleInitialized = channelID > 0;
    if (!moduleInitialized) {
        ERROR(F("ThingSpeak configuration requires a positive channelID."));
    }

    free(json);
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
