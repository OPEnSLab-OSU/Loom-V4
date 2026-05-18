#include "Loom_ThingSpeak.h"
#include "Logger.h"

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
    strncpy(this->username, broker_user, 100);
    strncpy(this->password, broker_pass, 100);
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
    char topic[MAX_TOPIC_LENGTH];
    if (moduleInitialized) {
        // TIMER_DISABLE;

        MemPool::Lease messageLease = manInst->getPool().allocLease(MAX_JSON_SIZE, "things_msg");
        if (!messageLease) {
            ERROR(F("Failed to allocate memory-pool lease for ThingSpeak publish!"));
            FUNCTION_END;
            return false;
        }

        /* Attempt to connect to the broker if it fails we should just return */
        if (!connectToBroker()) {
            FUNCTION_END;
            return false;
        }

        /* Format the message we want to publish */
        if (!formatMessage(topic, messageLease.chars())) {
            ERROR(F("ThingSpeak publish message exceeded buffer capacity!"));
            FUNCTION_END;
            return false;
        }

        /* Publish the message to the given topic */
        if (!publishMessage(topic, messageLease.chars(), false, 0)) {
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
void Loom_ThingSpeak::addFunction(int fieldNumber, FloatReturnFuncDefs function) {
    functionsNoParam.push_back(std::make_pair(fieldNumber, function));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ThingSpeak::addFunction(int fieldNumber, FloatReturnFuncDefsWithParam function,
                                  int parameter) {
    functionsParam.push_back(std::make_tuple(fieldNumber, function, parameter));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_ThingSpeak::formatMessage(char topic[MAX_TOPIC_LENGTH], char message[MAX_JSON_SIZE]) {
    char tempBuffer[100];

    /* Clear all buffers */
    memset(tempBuffer, '\0', 100);
    memset(topic, '\0', MAX_TOPIC_LENGTH);
    memset(message, '\0', MAX_JSON_SIZE);

    /* Format the topic to publish to many fields at once */
    snprintf(topic, MAX_TOPIC_LENGTH, "channels/%i/publish", channelID);
    auto appendMessage = [&](const char *text) -> bool {
        if (text == nullptr) {
            text = "";
        }

        size_t used = strlen(message);
        if (used >= MAX_JSON_SIZE) {
            return false;
        }

        size_t remaining = MAX_JSON_SIZE - used - 1;
        strncat(message, text, remaining);
        return strlen(text) <= remaining;
    };

    // Check if combined size of both function lists is more than 8
    if (functionsNoParam.size() + functionsParam.size() > 8) {
        WARNING(F("There have been more than 8 fields added. ThingSpeak only supports up to 8 "
                  "fields so any fields after the initial 8 will be ignored"));
    }

    /* Use the same variable for both loops, first looping over the list of functions with no
     * parameters, also check if we are still less than 8 fields */
    int i;
    int totalAdded = 0; // Track the number of fields that have been added
    for (i = 0; i < functionsNoParam.size() && totalAdded < 8; i++) {
        // Set the field number and then call the corresponding function to update
        snprintf(tempBuffer, 100, "field%i=%f&", functionsNoParam[i].first,
                 functionsNoParam[i].second());
        if (!appendMessage(tempBuffer)) {
            return false;
        }
        totalAdded++;
    }

    /* Next do the same thing except with list of functions that have a single integer paramater,
     * also check if we are still less than 8 fields*/
    for (i = 0; i < functionsParam.size() && totalAdded < 8; i++) {
        // Set the field number and then call the corresponding function to update
        snprintf(tempBuffer, 100, "field%i=%f&", std::get<0>(functionsParam[i]),
                 std::get<1>(functionsParam[i])(std::get<2>(functionsParam[i])));
        if (!appendMessage(tempBuffer)) {
            return false;
        }
        totalAdded++;
    }

    /* Check if we have a timestamp property if so we want to add a "created_at" paramater to the
     * message that we are publishing */
    if (!manInst->getDocument()["timestamp"].isNull()) {
        snprintf(tempBuffer, 100, "created_at=%s&",
                 manInst->getDocument()["timestamp"]["time_local"].as<const char *>());
        if (!appendMessage(tempBuffer)) {
            return false;
        }
    }

    /* Finally end the message with the status */
    return appendMessage("status=MQTTPUBLISH");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ThingSpeak::loadConfigFromJSON(const char *json) {
    FUNCTION_START;
    char output[OUTPUT_SIZE];

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

    memset(username, '\0', 100);
    memset(password, '\0', 100);

    if (!doc["channelID"].isNull())
        channelID = doc["channelID"].as<int>();

    if (!doc["clientID"].isNull())
        setClientID(doc["clientID"].as<const char *>());

    if (!doc["username"].isNull())
        strncpy(username, doc["username"].as<const char *>(), 100);

    if (!doc["password"].isNull())
        strncpy(password, doc["password"].as<const char *>(), 100);

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
