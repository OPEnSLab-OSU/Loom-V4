#include "Loom_RemoteManager.h"
#include "Logger.h"
#include "Loom_Manager.h"
#include "Hardware/Loom_Hypnos/Loom_Hypnos.h"
#include "Utilities/Loom_RemoteConfig.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_RemoteManager::Loom_RemoteManager(Manager &man, NetworkComponent &internet_client,
                                       const char *broker_address, int broker_port,
                                       const char *broker_user, const char *broker_pass)
    : MQTTComponent("RemoteManager", internet_client), manager(&man) {
    strncpy(this->address, broker_address ? broker_address : "", sizeof(this->address) - 1);
    this->address[sizeof(this->address) - 1] = '\0';
    port = broker_port;
    setBrokerCredentials(broker_user, broker_pass);
    manager->registerModule(this);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_RemoteManager::Loom_RemoteManager(Manager &man, NetworkComponent &internet_client)
    : MQTTComponent("RemoteManager", internet_client), manager(&man) {
    moduleInitialized = false; // SD credentials must arrive before any status/command exchange.
    manager->registerModule(this);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::power_up() {
    // Connect to the MQTT broker
    if (connectToBroker()) {
        publish();
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::power_down() {
    // Disconnect from the broker
    disconnectFromBroker();
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_RemoteManager::publish() {
    char topic[TOPIC_SIZE];
    char message[RETAINED_MESSAGE_SIZE];
    StaticJsonDocument<JSON_OBJECT_SIZE(4)> tempDoc;

    const bool onlinePublished = updateDeviceStatus(true);

    if (hypnos != nullptr) {

        updateHypnosInterval(topic, message, tempDoc);

        updateHypnosTime(topic, message);
    }

    // Always publish the offline status, including when the online publish failed.
    const bool offlinePublished = updateDeviceStatus(false);
    return onlinePublished && offlinePublished;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::loadConfigFromJSON(char *json) {
    FUNCTION_START;

    if (json == nullptr) {
        ERROR(F("Cannot load RemoteManager credentials from a null buffer."));
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

    memset(address, '\0', sizeof(address));
    port = 0;

    if (!doc["broker"].isNull()) {
        const char *broker = doc["broker"].as<const char *>();
        strncpy(address, broker ? broker : "", sizeof(address) - 1);
    }
    address[sizeof(address) - 1] = '\0';

    setBrokerCredentials(doc["username"] | "", doc["password"] | "");

    if (!doc["port"].isNull()) {
        port = doc["port"].as<int>();
    }

    moduleInitialized = address[0] != '\0' && port > 0;
    if (!moduleInitialized) {
        ERROR(F("RemoteManager configuration requires a broker address and positive port."));
    }

    free(json);
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::updateHypnosInterval(char topic[TOPIC_SIZE],
                                              char message[RETAINED_MESSAGE_SIZE],
                                              StaticJsonDocument<JSON_OBJECT_SIZE(4)> &json) {
    topic[0] = '\0';
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
    const int topicLength =
        snprintf(topic, TOPIC_SIZE, "RemoteManager/%s%i/Hypnos/setSleepInterval",
                 manager->get_device_name(), manager->get_instance_num());
    if (topicLength < 0 || static_cast<size_t>(topicLength) >= TOPIC_SIZE) {
        ERROR(F("Remote sleep-command topic does not fit."));
        return;
    }
    if (!getCurrentRetained(topic, message, RETAINED_MESSAGE_SIZE)) {
        return;
    }

    const DeserializationError error = deserializeJson(json, message);
    if (error != DeserializationError::Ok) {
        ERRORF("Invalid retained Hypnos interval JSON: %s", error.c_str());
        return;
    }
    int32_t intervalSeconds = 0;
    if (!loomRemote::parseSleepInterval(json.as<JsonObjectConst>(), intervalSeconds)) {
        ERROR(F("Retained Hypnos interval has invalid types, ranges, or duration."));
        return;
    }
    if (!hypnos->scheduleWake(TimeSpan(intervalSeconds))) {
        ERROR(F("Remote wake alarm could not be verified; retaining the request for retry."));
        return;
    }

    // Acknowledge only after validating and applying the interval.
    deleteRetained(topic);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_RemoteManager::updateHypnosTime(char topic[TOPIC_SIZE],
                                          char message[RETAINED_MESSAGE_SIZE]) {
    topic[0] = '\0';
    memset(message, '\0', RETAINED_MESSAGE_SIZE);

    // The retained message's presence requests a clock update; its payload is unused.
    const int topicLength = snprintf(topic, TOPIC_SIZE, "RemoteManager/%s%i/Hypnos/setRTC",
                                     manager->get_device_name(), manager->get_instance_num());
    if (topicLength < 0 || static_cast<size_t>(topicLength) >= TOPIC_SIZE) {
        ERROR(F("Remote clock-command topic does not fit."));
        return;
    }
    if (!getCurrentRetained(topic, message, RETAINED_MESSAGE_SIZE)) {
        return;
    }
    if (!hypnos->networkTimeUpdate()) {
        ERROR(F("Remote RTC update failed; retaining the request for a later retry."));
        return;
    }

    // Acknowledge only after the clock update succeeds.
    deleteRetained(topic);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_RemoteManager::updateDeviceStatus(bool onOff) {
    char topic[TOPIC_SIZE];
    const int topicLength = snprintf(topic, sizeof(topic), "RemoteManager/%s%i/status",
                                     manager->get_device_name(), manager->get_instance_num());
    if (topicLength < 0 || static_cast<size_t>(topicLength) >= sizeof(topic)) {
        ERROR(F("Remote status topic does not fit."));
        return false;
    }
    // There are only two status messages. Literal text keeps their JSON bytes unchanged without
    // building a document and a second character buffer for each status update.
    return publishMessage(topic, onOff ? "{\"online\":true}" : "{\"online\":false}");
}
////////////////////////////////////////////////////////////////////////////////////////////////////
