#include "Heartbeat.h"
#include "Logger.h"


Loom_Heartbeat::Loom_Heartbeat(uint32_t normalWorkInterval, 
                          Manager* managerInstance, 
                          Loom_Hypnos* hypnosInstance,
                          Loom_LoRa* loraInstance) 
            : heartbeatDoc(1024),
              normalWorkInterval(normalWorkInterval),
              managerInstance(managerInstance),
              hypnosInstance(hypnosInstance),
              loraInstance(loraInstance) {
}

bool Loom_Heartbeat::getHeartbeatFlag() {
    // if normalWorkInterval is 1 or less always do normal work transmission
    if (normalWorkInterval <= 1) {
        LOGF("[HEARTBEAT] Normal work interval 1 or less, heartbeat flag FALSE");
        return false;
    } 

    // if currentInterval equals 0, that means this is the first transmission
    // so transmit normal work
    // if currentInterval is equal to normalWorkInterval, or an edge case
    // occurs were currentInterval is greater than normalWorkInterval,
    // then transmit normal work
    if (currentInterval == 0 || currentInterval >= normalWorkInterval) {
        LOGF("[HEARTBEAT] Normal work interval active, heartbeat flag FALSE");
        currentInterval = 1;
        return false;
    }

    // otherwise increment the interval and return the heartbeat flag
    // indicating to transmit a heartbeat packet
    LOGF("[HEARTBEAT] Heartbeat interval active, heartbeat flag TRUE");
    currentInterval++;
    return true;
}

void Loom_Heartbeat::createDoc() {
    LOGF("[HEARTBEAT] Creating heartbeat JSON packet");
    heartbeatDoc.clear();

    heartbeatDoc["type"] = "heartbeat";

    JsonObject objNestedId = heartbeatDoc.createNestedObject("id");
    objNestedId["name"] = managerInstance->get_device_name();
    objNestedId["instance"] = managerInstance->get_instance_num();

    // heartbeatDoc["battery_voltage"] = Loom_Analog::getBatteryVoltage();
    // label battery voltage as analog Vbat and round it to 2 places
    JsonObject battery = heartbeatDoc.createNestedObject("analog");
    battery["Vbat"] = roundf(Loom_Analog::getBatteryVoltage() * 100.0f) / 100.0f;

    if(hypnosInstance != nullptr) {
        char utcTimeStr[21];
        char localTimeStr[21];
        DateTime utcTime = hypnosInstance->getCurrentTime();
        DateTime localTime = hypnosInstance->getLocalTime(utcTime);
        hypnosInstance->dateTime_toString(utcTime, utcTimeStr);
        hypnosInstance->dateTime_toString(localTime, localTimeStr, true); // set third arg to true for local time format

        JsonObject objNestedTimestamp = heartbeatDoc.createNestedObject("timestamp");
        objNestedTimestamp["time_utc"] = utcTimeStr;
        objNestedTimestamp["time_local"] = localTimeStr;
    }
}

void Loom_Heartbeat::addData(const char* module, const char* dataName, const char* data) {
    JsonObject dataObj;

    if (heartbeatDoc.containsKey(module)) {
        dataObj = heartbeatDoc[module].as<JsonObject>();
    } else {
        dataObj = heartbeatDoc.createNestedObject(module);
    }

    dataObj[dataName] = data;
}

DynamicJsonDocument& Loom_Heartbeat::getDoc() {
    return heartbeatDoc;
}

void Loom_Heartbeat::makeHeartbeat() {
    createDoc();
}

bool Loom_Heartbeat::transmit(const uint8_t destinationAddress) {
    LOGF("[HEARTBEAT] Transmitting heartbeat packet");
    return loraInstance->send(destinationAddress, heartbeatDoc.as<JsonObject>());
}

bool Loom_Heartbeat::transmitCustom(const uint8_t destinationAddress, JsonDocument& document) {
    LOGF("[HEARTBEAT] Transmitting custom heartbeat packet");
    return loraInstance->send(destinationAddress, document.as<JsonObject>());
}