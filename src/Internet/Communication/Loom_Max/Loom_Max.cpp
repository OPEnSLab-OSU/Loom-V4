#include "Loom_Max.h"
#include "Logger.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Max::Loom_Max(Manager &man, Loom_WIFI &wifi)
    : Module("Max Pub/Sub"), manager(&man), wifiModule(&wifi) {
    wifi.useMax();
    manager->registerModule(this);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
;

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Max::~Loom_Max() {
    udpSend.stop();
    udpRecv.stop();

    // Clean up the actuator instances
    for (Actuator *actuator : actuators) {
        delete actuator;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Max::package() {
    JsonArray ipFields = manager->getDocument()["id"].createNestedArray("ip");
    IPAddress ip = wifiModule->getIPAddress();
    ipFields.add(ip[0]);
    ipFields.add(ip[1]);
    ipFields.add(ip[2]);
    ipFields.add(ip[3]);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Max::initialize() {
    LOG(F("Initializing Max Communication...."));

    // Set the IP and port to communicate over
    setIP();
    setUDPPort();

    LOG(F("Connections Opened!"));

    if (!actuators.empty()) {
        LOG(F("Initializing desired actuators..."));
        for (Actuator *actuator : actuators) {
            actuator->initialize();
        }
        LOG(F("Successfully initialized actuators!"));
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Max::publish() {
    char ip[16] = {};

    // Print the device IP
    wifiModule->ipToString(remoteIP, ip);
    LOGF("Sending packet to %s:%u", ip, sendPort);

    // Attempt to start a new packet
    if (udpSend.beginPacket(remoteIP, sendPort) != 1) {
        ERROR(F("The IP address or port were invalid!"));
        return false;
    }

    // Package all actuators
    for (Actuator *actuator : actuators) {
        actuator->package(manager->get_data_object(actuator->getModuleName()));
    }

    const size_t size = serializeJson(manager->getDocument(), udpSend);

    if (size == 0) {
        ERROR(F("An error occurred when attempting to write the JSON packet to the UDP stream"));
        return false;
    }

    if (udpSend.endPacket() != 1) {
        ERROR(F("An error occurred when attempting to close the current packet!"));
        return false;
    }

    LOG(F("Packet successfully sent!"));
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Max::dispatchActuatorCommand(JsonVariant command) {
    const char *type = command["module"].as<const char *>();
    if (type == nullptr) {
        return;
    }

    JsonArray parameters = command["params"].as<JsonArray>();
    const int instanceNum = parameters[0].as<int>();
    // Relay and Neopixel commands target every matching module; Servo and Stepper use params[0].
    const bool allInstances =
        strstr(type, "Relay") != nullptr || strstr(type, "Neopixel") != nullptr;
    for (Actuator *actuator : actuators) {
        if (strcmp(actuator->typeToString(), type) != 0) {
            continue;
        }
        if (!allInstances && actuator->get_instance_num() != instanceNum) {
            continue;
        }
        actuator->control(parameters);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Max::subscribe() {
    if (!udpRecv.parsePacket()) {
        WARNING(F("No message received!"));
        return false;
    }

    // subscribe() follows publish(); the next manager.package() rebuilds the outgoing sample.
    // Reuse its existing JSON pool instead of retaining a second receive pool.
    DynamicJsonDocument &messageJson = manager->getDocument();
    messageJson.clear();
    const DeserializationError error = deserializeJson(messageJson, udpRecv);
    if (error != DeserializationError::Ok) {
        ERRORF("Failed to parse JSON data from UDP stream, Error: %s", error.c_str());
        messageJson.clear();
        return false;
    }

    if (!actuators.empty()) {
        const char *messageType = messageJson["type"].as<const char *>();
        if (messageType != nullptr && strcmp(messageType, "command") == 0) {
            for (JsonVariant command : messageJson["commands"].as<JsonArray>()) {
                dispatchActuatorCommand(command);
            }
        }
        return true;
    }

    char ip[16] = {};
    wifiModule->ipToString(udpRecv.remoteIP(), ip);
    LOGF("Packet received from: %s", ip);
    LOG(F("Message Json: "));
    serializeJsonPretty(messageJson, Serial);
    Serial.println();

    JsonVariant command = messageJson["commands"][0];
    const char *commandModule = command["module"].as<const char *>();
    if (commandModule != nullptr && strstr(commandModule, "MaxSub") != nullptr &&
        command["func"].as<int>() == 99) {
        const char *name = command["params"][0].as<const char *>();
        const char *password = command["params"][1].as<const char *>();
        wifiModule->storeNewWiFiCreds(name, password);
    }
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Max::setUDPPort() {
    sendPort = SEND_BASE_UDP_PORT + manager->get_instance_num();
    recvPort = RECV_BASE_UDP_PORT + manager->get_instance_num();

    // Open a listen server on the specified port
    udpSend.begin(sendPort);
    udpRecv.begin(recvPort);

    LOGF("Listening for UDP Packets on %u", recvPort);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Max::setIP() { remoteIP = wifiModule->getBroadcast(); }
////////////////////////////////////////////////////////////////////////////////////////////////////
