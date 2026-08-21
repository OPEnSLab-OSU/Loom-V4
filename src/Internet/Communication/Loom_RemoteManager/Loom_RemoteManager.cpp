#include "Loom_RemoteManager.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_RemoteManager::Loom_RemoteManager(Manager &man, NetworkComponent &internet_client,
                                       const char *broker_address, int broker_port,
                                       const char *broker_user, const char *broker_pass)
    : MQTTComponent("RemoteManager", internet_client), manInst(&man) {
    strncpy(this->address, broker_address ? broker_address : "", sizeof(this->address) - 1);
    this->address[sizeof(this->address) - 1] = '\0';
    port = broker_port;
    setBrokerCredentials(broker_user, broker_pass);
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_RemoteManager::Loom_RemoteManager(Manager &man, NetworkComponent &internet_client)
    : MQTTComponent("RemoteManager", internet_client), manInst(&man) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::power_up() {
    // Connect to the MQTT broker
    if (connectToBroker()) {
        publish();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::power_down() {
    // Disconnect from the broker
    disconnectFromBroker();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_RemoteManager::publish() {
    // Create topic name buffer and message buffer as well as a temp JSON document to parse the
    // received packets into
    char topic[MAX_TOPIC_LENGTH];
    char message[RETAINED_MESSAGE_SIZE];
    StaticJsonDocument<JSON_OBJECT_SIZE(4)> tempDoc;

    // Update the current device status
    bool success = updateDeviceStatus(true);

    // Check if we are using a hypnos and then update the parameters about the hypnos
    if (hypnosInst != nullptr) {

        // Update the hypnos sleep interval if there is anything to update
        updateHypnosInterval(topic, message, tempDoc);

        // Update the RTC time, if desired
        updateHypnosTime(topic, message);
    }

    success = updateDeviceStatus(false) && success;

    return success;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::loadConfigFromJSON(char *json) {
    FUNCTION_START;

    if (json == nullptr) {
        ERROR(F("Cannot load RemoteManager credentials from a null buffer."));
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

    // Clear the strings and set port = 0
    memset(address, '\0', 100);
    port = 0;

    /* We should check if any parameter is null */
    if (!doc["broker"].isNull()) {
        const char *broker = doc["broker"].as<const char *>();
        strncpy(address, broker ? broker : "", sizeof(address) - 1);
    }
    address[sizeof(address) - 1] = '\0';

    setBrokerCredentials(doc["username"] | "", doc["password"] | "");

    if (!doc["port"].isNull())
        port = doc["port"].as<int>();

    moduleInitialized = address[0] != '\0' && port > 0;
    if (!moduleInitialized)
        ERROR(F("RemoteManager configuration requires a broker address and positive port."));

    free(json);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::updateHypnosInterval(char topic[MAX_TOPIC_LENGTH],
                                              char message[RETAINED_MESSAGE_SIZE],
                                              StaticJsonDocument<JSON_OBJECT_SIZE(4)> &json) {
    // Clear message and topic and json
    memset(topic, '\0', MAX_TOPIC_LENGTH);
    memset(message, '\0', RETAINED_MESSAGE_SIZE);
    json.clear();

    /*
        This is the topic that we need to publish to to change the sleep interval.
        The packet published here by the remote management interface should be as follows
        {
            "days": 0,
            "hours": 0,
            "minutes": 0,
            "seconds": 0
        }
    */
    snprintf(topic, MAX_TOPIC_LENGTH, "RemoteManager/%s%i/Hypnos/setSleepInterval",
             manInst->get_device_name(), manInst->get_instance_num());
    if (getCurrentRetained(topic, message, RETAINED_MESSAGE_SIZE)) {

        // Parse the incoming message into a JSON Document and then create a new date time from the
        // values to update the current time in the Hypnos
        const DeserializationError error = deserializeJson(json, message);
        if (error != DeserializationError::Ok) {
            ERRORF("Invalid retained Hypnos interval JSON: %s", error.c_str());
            return;
        }
        const int days = json["days"] | 0;
        const int hours = json["hours"] | 0;
        const int minutes = json["minutes"] | 0;
        const int seconds = json["seconds"] | 0;
        if (days < 0 || days > 24854 || hours < 0 || hours > 23 || minutes < 0 || minutes > 59 ||
            seconds < 0 || seconds > 59) {
            ERROR(F("Retained Hypnos interval contains an out-of-range field."));
            return;
        }

        const TimeSpan time(static_cast<int16_t>(days), static_cast<int8_t>(hours),
                            static_cast<int8_t>(minutes), static_cast<int8_t>(seconds));

        if (time.totalseconds() <= 0) {
            ERROR(F("Retained Hypnos interval must be greater than zero."));
            return;
        }

        // Set the new interrupt duration
        hypnosInst->setInterruptDuration(time);

        // And then delete the current retained message so we don't update it again
        deleteRetained((const char *)topic);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::updateHypnosTime(char topic[MAX_TOPIC_LENGTH],
                                          char message[RETAINED_MESSAGE_SIZE]) {
    // Clear message and topic
    memset(topic, '\0', MAX_TOPIC_LENGTH);
    memset(message, '\0', RETAINED_MESSAGE_SIZE);

    /*
        This is the topic that we need to publish to to change the RTC time. The contents of the
       packet can be whatever there just needs to be a packet
    */
    snprintf(topic, MAX_TOPIC_LENGTH, "RemoteManager/%s%i/Hypnos/setRTC",
             manInst->get_device_name(), manInst->get_instance_num());
    if (getCurrentRetained(topic, message, RETAINED_MESSAGE_SIZE)) {
        // Set the new RTC time from the network
        if (!hypnosInst->networkTimeUpdate()) {
            ERROR(F("Remote RTC update failed; retaining the request for a later retry."));
            return;
        }

        // And then delete the current retained message so we don't update it again
        deleteRetained((const char *)topic);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_RemoteManager::updateDeviceStatus(bool onOff) {
    StaticJsonDocument<JSON_OBJECT_SIZE(1)> json;
    char message[100];
    char topic[MAX_TOPIC_LENGTH];

    // Clear strings
    memset(message, '\0', 100);
    memset(topic, '\0', MAX_TOPIC_LENGTH);

    // Set the online flag and then serialize the json to a string
    json["online"] = onOff;
    serializeJson(json, message, sizeof(message));

    // Format the topic to publish the data to and publish the message
    snprintf(topic, MAX_TOPIC_LENGTH, "RemoteManager/%s%i/status", manInst->get_device_name(),
             manInst->get_instance_num());
    return publishMessage(topic, message);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
