#include "Loom_ThingSpeak.h"
#include "Logger.h"
#include "Loom_Manager.h"
#include "Utilities/Loom_ThingSpeakPayload.h"

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
    : Loom_ThingSpeak(man, internet_client, 0, "", "", "") {
    // SD credentials arrive later, but the broker address and port are needed in both paths.
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

    char topic[TOPIC_SIZE];
    const int topicLength = snprintf(topic, sizeof(topic), "channels/%i/publish", channelID);
    if (topicLength < 0 || static_cast<size_t>(topicLength) >= sizeof(topic)) {
        ERROR(F("ThingSpeak topic does not fit its fixed buffer."));
        return false;
    }

    // Capture each reading once, preserving the historical grouping/order. The payload holds
    // these values while the stream formats small pieces; it does not keep a second JSON packet.
    loomThingSpeak::Payload payload;
    for (size_t i = 0; i < fieldCount; ++i) {
        const FieldFunction &field = fields[i];
        if (field.readValue != nullptr && !payload.addField(field.fieldNumber, field.readValue())) {
            return false;
        }
    }
    for (size_t i = 0; i < fieldCount; ++i) {
        const FieldFunction &field = fields[i];
        if (field.readParameterizedValue != nullptr &&
            !payload.addField(field.fieldNumber, field.readParameterizedValue(field.parameter))) {
            return false;
        }
    }
    const char *localTime = manager->getDocument()["timestamp"]["time_local"].as<const char *>();
    if (!payload.prepare(localTime)) {
        ERROR(F("ThingSpeak message could not be formatted safely."));
        return false;
    }
    FUNCTION_END;
    return publishStream(topic, payload, payload.length(), false, 0);
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
    if (readValue == nullptr || fieldNumber < 1 || fieldNumber > static_cast<int>(MAX_FIELDS)) {
        ERROR(F("ThingSpeak requires a field from 1 through 8 and a non-null function."));
        return;
    }
    if (fieldCount >= MAX_FIELDS) {
        WARNING(F("ThingSpeak supports at most eight fields; field was not retained."));
        return;
    }
    fields[fieldCount++] = {fieldNumber, readValue, nullptr, 0};
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ThingSpeak::addFunction(int fieldNumber, FloatReturnFuncDefsWithParam readValue,
                                  int parameter) {
    if (readValue == nullptr || fieldNumber < 1 || fieldNumber > static_cast<int>(MAX_FIELDS)) {
        ERROR(F("ThingSpeak requires a field from 1 through 8 and a non-null function."));
        return;
    }
    if (fieldCount >= MAX_FIELDS) {
        WARNING(F("ThingSpeak supports at most eight fields; field was not retained."));
        return;
    }
    fields[fieldCount++] = {fieldNumber, nullptr, readValue, parameter};
}
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
