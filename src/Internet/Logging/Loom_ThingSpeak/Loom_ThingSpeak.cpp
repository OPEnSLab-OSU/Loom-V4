#include "Loom_ThingSpeak.h"
#include "Logger.h"

namespace {
bool appendText(char *destination, size_t capacity, const char *suffix) {
    if (destination == nullptr || suffix == nullptr || capacity == 0)
        return false;

    const size_t used = strnlen(destination, capacity);
    const size_t suffixLength = strlen(suffix);
    if (used >= capacity || suffixLength > capacity - used - 1)
        return false;

    memcpy(destination + used, suffix, suffixLength + 1);
    return true;
}
} // namespace

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ThingSpeak::Loom_ThingSpeak(Manager &man, NetworkComponent &internet_client, int channelID,
                                 const char *clientID, const char *broker_user,
                                 const char *broker_pass)
    : MQTTComponent("ThingSpeak", internet_client), manInst(&man) {
    /* Thing speak server parameters */
    strncpy(this->address, "mqtt3.thingspeak.com", 100);
    port = 1883;

    /* ThingSpeak provided connection details */
    setClientID(clientID);
    setBrokerCredentials(broker_user, broker_pass);
    this->channelID = channelID;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ThingSpeak::Loom_ThingSpeak(Manager &man, NetworkComponent &internet_client)
    : MQTTComponent("ThingSpeak", internet_client), manInst(&man) {
    moduleInitialized = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_ThingSpeak::publish() {
    FUNCTION_START;
    char message[MESSAGE_SIZE];
    char topic[MAX_TOPIC_LENGTH];
    if (moduleInitialized) {
        // TIMER_DISABLE;

        /* Attempt to connect to the broker if it fails we should just return */
        if (!connectToBroker()) {
            FUNCTION_END;
            return false;
        }

        /* Format the message we want to publish */
        if (!formatMessage(topic, message)) {
            ERROR(F("ThingSpeak message exceeded its fixed buffer."));
            FUNCTION_END;
            return false;
        }

        /* Publish the message to the given topic */
        if (!publishMessage(topic, message, false, 0)) {
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
bool Loom_ThingSpeak::publish(Loom_BatchSD &batchSD) {
    (void)batchSD;
    ERROR(F("ThingSpeak batch replay is not supported by the callback-based field API."));
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ThingSpeak::addFunction(int fieldNumber, FloatReturnFuncDefs function) {
    if (function == nullptr) {
        ERROR(F("Cannot add a null ThingSpeak field function."));
        return;
    }
    if (functionsNoParam.size() + functionsParam.size() >= 8) {
        WARNING(F("ThingSpeak supports at most eight fields; field was not retained."));
        return;
    }
    functionsNoParam.push_back(std::make_pair(fieldNumber, function));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ThingSpeak::addFunction(int fieldNumber, FloatReturnFuncDefsWithParam function,
                                  int parameter) {
    if (function == nullptr) {
        ERROR(F("Cannot add a null ThingSpeak field function."));
        return;
    }
    if (functionsNoParam.size() + functionsParam.size() >= 8) {
        WARNING(F("ThingSpeak supports at most eight fields; field was not retained."));
        return;
    }
    functionsParam.push_back(std::make_tuple(fieldNumber, function, parameter));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_ThingSpeak::formatMessage(char topic[MAX_TOPIC_LENGTH], char message[MESSAGE_SIZE]) {
    char tempBuffer[100];

    /* Clear all buffers */
    memset(tempBuffer, '\0', 100);
    memset(topic, '\0', MAX_TOPIC_LENGTH);
    memset(message, '\0', MESSAGE_SIZE);

    /* Format the topic to publish to many fields at once */
    snprintf(topic, MAX_TOPIC_LENGTH, "channels/%i/publish", channelID);

    // Check if combined size of both function lists is more than 8
    if (functionsNoParam.size() + functionsParam.size() > 8) {
        WARNING(F("There have been more than 8 fields added. ThingSpeak only supports up to 8 "
                  "fields so any fields after the initial 8 will be ignored"));
    }

    /* Use the same variable for both loops, first looping over the list of functions with no
     * parameters, also check if we are still less than 8 fields */
    size_t i;
    int totalAdded = 0; // Track the number of fields that have been added
    for (i = 0; i < functionsNoParam.size() && totalAdded < 8; i++) {
        // Set the field number and then call the corresponding function to update
        snprintf(tempBuffer, 100, "field%i=%f&", functionsNoParam[i].first,
                 functionsNoParam[i].second());
        if (!appendText(message, MESSAGE_SIZE, tempBuffer))
            return false;
        totalAdded++;
    }

    /* Next do the same thing except with a list of functions that have one integer parameter,
     * also check if we are still less than 8 fields*/
    for (i = 0; i < functionsParam.size() && totalAdded < 8; i++) {
        // Set the field number and then call the corresponding function to update
        snprintf(tempBuffer, 100, "field%i=%f&", std::get<0>(functionsParam[i]),
                 std::get<1>(functionsParam[i])(std::get<2>(functionsParam[i])));
        if (!appendText(message, MESSAGE_SIZE, tempBuffer))
            return false;
        totalAdded++;
    }

    /* Check if we have a timestamp property; if so, add a "created_at" parameter to the
     * message that we are publishing */
    if (!manInst->getDocument()["timestamp"].isNull()) {
        const char *localTime =
            manInst->getDocument()["timestamp"]["time_local"].as<const char *>();
        if (localTime != nullptr) {
            snprintf(tempBuffer, sizeof(tempBuffer), "created_at=%s&", localTime);
            if (!appendText(message, MESSAGE_SIZE, tempBuffer))
                return false;
        }
    }

    /* Finally end the message with the status */
    return appendText(message, MESSAGE_SIZE, "status=MQTTPUBLISH");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ThingSpeak::loadConfigFromJSON(char *json) {
    FUNCTION_START;

    if (json == nullptr) {
        ERROR(F("Cannot load ThingSpeak credentials from a null buffer."));
        moduleInitialized = false;
        FUNCTION_END;
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
        FUNCTION_END;
        return;
    }

    if (!doc["channelID"].isNull())
        channelID = doc["channelID"].as<int>();

    if (!doc["clientID"].isNull())
        setClientID(doc["clientID"].as<const char *>());

    setBrokerCredentials(doc["username"] | "", doc["password"] | "");

    moduleInitialized = channelID > 0;
    if (!moduleInitialized)
        ERROR(F("ThingSpeak configuration requires a positive channelID."));

    free(json);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
