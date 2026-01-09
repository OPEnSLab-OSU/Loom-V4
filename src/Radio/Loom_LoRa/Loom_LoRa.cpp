#include "Loom_LoRa.h"
#include "ArduinoJson.hpp"
#include "FatLib/ArduinoFiles.h"
#include "Logger.h"
#include "Module.h"
#include <cstdint>
#include <cstdio>

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LoRa::Loom_LoRa(
    Manager& manager,
    const uint8_t address, 
    const uint8_t powerLevel,
    const uint8_t sendMaxRetries,
    const uint8_t receiveMaxRetries,
    const uint16_t retryTimeout
) :     Module("LoRa"),
        manager(&manager), 
        radioDriver{RFM95_CS, RFM95_INT},
        deviceAddress(address),
        powerLevel(powerLevel),
        sendRetryCount(sendMaxRetries),
        receiveRetryCount(receiveMaxRetries),
        retryTimeout(retryTimeout),
        expectedOutstandingPackets(0)
{
    this->radioManager = new RHReliableDatagram(
        radioDriver, this->deviceAddress);
    this->manager->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LoRa::Loom_LoRa(
    Manager& manager,
    const uint8_t powerLevel, 
    const uint8_t retryCount, 
    const uint16_t retryTimeout
) : Loom_LoRa(
    manager, 
    manager.get_instance_num(), 
    powerLevel, 
    retryCount, 
    retryCount,
    retryTimeout
) {}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LoRa::~Loom_LoRa() {
    delete radioManager;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::initialize() {
    // Set CS pin as pull up
    pinMode(RFM95_CS, INPUT_PULLUP);
    
    // Reset the radio
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);

    // Initialize the radio manager
    if (radioManager->init()) {
        LOG(F("Radio manager successfully initialized!"));

    } else {
        ERROR(F("Radio manager failed to initialize!"));
        moduleInitialized = false;
        return;
    }

    // Set the radio frequency
    if (radioDriver.setFrequency(RF95_FREQ)) {
        LOGF("Radio frequency successfully set to: %f", RF95_FREQ);
    } else {
        ERROR(F("Failed to set frequency!"));
        moduleInitialized = false;
        return;
    }

    // Set radio power level
    LOGF("Setting device power level to: %i", powerLevel);
    radioDriver.setTxPower(powerLevel, false);

    // Set timeout time
    LOGF("Timeout time set to: %i,", retryTimeout);
    radioManager->setTimeout(retryTimeout);

    // Set retry attempts
    LOGF("Transmit retry count set to: %i", sendRetryCount);
    radioManager->setRetries(sendRetryCount);

    // Print the set address of the device
    LOGF("Address set to: %i", radioManager->thisAddress());
    
    // https://cdn.sparkfun.com/assets/a/e/7/e/b/RFM95_96_97_98W.pdf, Page 22

    // Set bandwidth
    radioDriver.setSignalBandwidth(125000);

    // Higher spreading factors give us more range
    radioDriver.setSpreadingFactor(7); 

    // Coding rate should be 4/5
    radioDriver.setCodingRate4(5);	
    radioDriver.sleep();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::power_up() {
    if (batchSD) {
        int currentBatch = batchSD->getCurrentBatch();
        int batchSize = batchSD->getBatchSize();
        poweredUp = currentBatch == batchSize - 1;
    }

    if (poweredUp) {
        radioDriver.available();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::power_down() {
    if (poweredUp) {
        radioDriver.sleep();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::package() {
    if (!moduleInitialized) {
        return;
    }

    JsonObject json = manager->get_data_object(getModuleName());
    json["RSSI"] = signalStrength;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::setAddress(const uint8_t newAddress) {
    deviceAddress = newAddress;
    radioManager->setThisAddress(newAddress);
    radioDriver.sleep();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receiveFromLoRa(uint8_t *buf, uint8_t buf_size, 
                                       uint timeout, uint8_t *fromAddress) {
    bool status = true;

    memset(buf, 0, buf_size);

    LOG(F("Waiting for message..."));

    if (timeout) {
        status = radioManager->recvfromAckTimeout(buf, &buf_size, timeout, 
                                                  fromAddress);
    } else {
        status = radioManager->recvfromAck(buf, &buf_size, 
                                           fromAddress);
    }

    if (!status) {
        WARNING(F("No message received"));
    }

    radioDriver.sleep();
    return status;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
FragReceiveStatus Loom_LoRa::receiveFrag(uint timeout, bool shouldProxy,
                                         uint8_t* fromAddress) {
    if (!moduleInitialized) {
        ERROR(F("LoRa module not initialized!"));
        return FragReceiveStatus::Error;
    }

    uint8_t buf[MAX_MESSAGE_LENGTH] = {};

    bool recvStatus = receiveFromLoRa(buf, sizeof(buf), timeout, fromAddress);
    if (!recvStatus) {
        return FragReceiveStatus::Error;
    }

    LOGF("Received packet from %i", *fromAddress);

    StaticJsonDocument<300> tempDoc;

    // cast buf to const to avoid mutation
    auto err = deserializeMsgPack(tempDoc, (const char *)buf, sizeof(buf));
    if (err != DeserializationError::Ok) {
        ERRORF("Error occurred parsing MsgPack: %s", err.c_str());
        return FragReceiveStatus::Error;
    }

    bool isReady = false;
    if (tempDoc.containsKey("batch_size")) {
        isReady = handleBatchHeader(tempDoc);

    } else if (tempDoc.containsKey("numPackets")) {
        isReady = handleFragHeader(tempDoc, *fromAddress);

    } else if (frags.find(*fromAddress) != frags.end()) {
        isReady = handleFragBody(tempDoc, *fromAddress);
    
    } else if (tempDoc.containsKey("module")) {
        isReady = handleLostFrag(tempDoc, *fromAddress);

    } else {
        isReady = handleSingleFrag(tempDoc);
    }

    if (isReady) {
        if (shouldProxy) {
            const char *name = manager->getDocument()["id"]["name"];
            manager->set_device_name(name);

            int instNum = manager->getDocument()["id"]["instance"];
            manager->set_instance_num(instNum);
        }

        if (expectedOutstandingPackets > 0) {
            expectedOutstandingPackets--;
        }

        return FragReceiveStatus::Complete;
    } else {
        return FragReceiveStatus::Incomplete;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleBatchHeader(JsonDocument &tempDoc) {
    int batch_size = tempDoc["batch_size"];
    LOGF("Received batch header, expecting %i packets", batch_size);
    expectedOutstandingPackets += batch_size;
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleFragHeader(JsonDocument &workingDoc, 
                                        uint8_t fromAddress) {
    int expectedFragCount = workingDoc["numPackets"].as<int>();
    workingDoc.remove("numPackets");

    int packetSpace = 300 * (expectedFragCount + 1);

    if (frags.find(fromAddress) != frags.end()) {
        WARNINGF("Dropping corrupted packet received from %i", fromAddress);

        frags.erase(fromAddress);
    }

    // this should never fail
    auto inserted = frags.emplace(std::make_pair(
        fromAddress,
        PartialPacket { 
            expectedFragCount, 
            DynamicJsonDocument(packetSpace) }));

    inserted.first->second.working = workingDoc;

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleFragBody(JsonDocument &workingDoc, 
                                      uint8_t fromAddress) {
    PartialPacket *partialPacket = &frags.find(fromAddress)->second;

    JsonArray contents = partialPacket->working["contents"].as<JsonArray>();
    contents.add(workingDoc);

    partialPacket->remainingFragments--;

    if (partialPacket->remainingFragments == 0) { 
        // overwrite the manager document by deep-copying the finalized packet
        manager->getDocument().set(partialPacket->working);
        frags.erase(fromAddress);

        return true;
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleSingleFrag(JsonDocument &workingDoc) {
    // overwrite the manager document by deep-copying the finalized packet
    manager->getDocument().set(workingDoc);
    
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::handleLostFrag(JsonDocument &workingDoc, 
                                      uint8_t fromAddress) {
    WARNINGF("Dropping fragmented packet body with no header received from %i",
             fromAddress);

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::receive(uint timeout, uint8_t* fromAddress, bool shouldProxy) {
    int retryCount = receiveRetryCount;
    while (retryCount > 0) {
        FragReceiveStatus status = receiveFrag(timeout, shouldProxy, fromAddress);

        switch (status) {
        case FragReceiveStatus::Complete:
            return true;
        case FragReceiveStatus::Error:
            retryCount--;
            break;
        }
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool Loom_LoRa::receive(uint timeout, bool shouldProxy) {
    uint8_t fromAddress;
    return receive(timeout, &fromAddress, shouldProxy);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::transmitToLoRa(JsonObject json, uint8_t destinationAddress) {
    uint8_t buffer[MAX_MESSAGE_LENGTH] = {};
    bool status = false;

    status = serializeMsgPack(json, buffer, MAX_MESSAGE_LENGTH);
    if (!status) {
        ERROR(F("Failed to convert JSON to MsgPack"));
        return false;
    }

    status = radioManager->sendtoWait(buffer, sizeof(buffer), 
                                      destinationAddress);
    if (!status) {
        ERROR(F("Failed to send packet to specified address!"));
        return false;
    }

    LOG(F("Successfully transmitted packet!"));
    signalStrength = radioDriver.lastRssi();
    radioDriver.sleep();
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendFullPacket(JsonObject json, uint8_t destinationAddress) {
    return transmitToLoRa(json, destinationAddress);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendFragmentedPacket(JsonObject json, 
                                     uint8_t destinationAddress) {
    LOG(F("Packet was greater than the maximum packet length; the packet will be fragmented"));
    bool status = false;

    status = json.containsKey("contents");
    if (!status) {
        ERROR(F("JSON data is malformed and cannot be fragmented"));
        return false;
    }
    int numFrags = json["contents"].size();

    status = sendPacketHeader(json, destinationAddress);
    if (!status) {
        ERROR(F("Unable to transmit initial packet header! Split packets will not be sent"));
        return false;
    }

    for (int i = 0; i < numFrags; i++) {
        LOGF("Sending fragmented packet (%i/%i)...", i+1, numFrags);

        JsonObject frag = json["contents"][i].as<JsonObject>();
        status = transmitToLoRa(frag, destinationAddress);
        if (!status) {
            ERROR(F("Failed to transmit fragmented packet!"));
            return false;
        }

        // randomizing the delay helps decrease collisions
        delay(random(400, 1000));
    }

    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendPacketHeader(JsonObject json, 
                                 uint8_t destinationAddress) {
    StaticJsonDocument<MAX_MESSAGE_LENGTH * 2> sendDoc;

    sendDoc["type"] = json["type"].as<const char*>();
    sendDoc["numPackets"] = json["contents"].size();
    
    JsonObject objId = sendDoc.createNestedObject("id");
    objId["name"] = json["id"]["name"].as<const char*>();
    objId["instance"] = json["id"]["instance"].as<int>();

    sendDoc.createNestedArray("contents");

    if (!json["timestamp"].isNull()) {
        JsonObject objTimestamp = sendDoc.createNestedObject("timestamp");
        objTimestamp["time_utc"] = 
            json["timestamp"]["time_utc"].as<const char*>();
        objTimestamp["time_local"] = 
            json["timestamp"]["time_local"].as<const char*>();
    }

    JsonObject sendOut = sendDoc.as<JsonObject>();
    return transmitToLoRa(sendOut, destinationAddress);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::heartbeatInit( const uint8_t newAddress, 
                    const uint32_t pHeartbeatInterval, 
                    const uint32_t pNormalWorkInterval,
                    Loom_Hypnos* hypnosInstance)
{
    if(hypnosInstance != nullptr && pHeartbeatInterval < 60) {
        WARNING(F("Heartbeat interval too low for Hypnos, setting to minimum of 60 seconds"));
        heartbeatInterval_s = 60;
    }
    else if(pHeartbeatInterval < 5) {
        WARNING(F("Heartbeat interval too low, setting to minimum of 5 seconds"));
        heartbeatInterval_s = 5;
    }
    else 
        heartbeatInterval_s = pHeartbeatInterval;

    if(pNormalWorkInterval < 5) {
        WARNING(F("Normal work interval too low, setting to minimum of 5 seconds"));
        normWorkInterval_s = 5;
    }
    else 
        normWorkInterval_s = pNormalWorkInterval;

    heartbeatDestAddress = newAddress;

    heartbeatTimer_s = heartbeatInterval_s;
    normWorkTimer_s = normWorkInterval_s;
    hypnosPtr = hypnosInstance;

    if(hypnosPtr != nullptr) {
        hypnosPtr->clearAlarms();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

TimeSpan Loom_LoRa::secondsToTimeSpan(const uint32_t totalSeconds) {
    // Build from d/h/m/s:
    uint16_t rem = 0;
    uint16_t days = totalSeconds / 86400;
    rem  = totalSeconds % 86400;
    uint8_t hours = rem / 3600;
    rem = rem % 3600;
    uint8_t minutes = rem / 60;
    uint8_t seconds = rem % 60;

    return TimeSpan(days, hours, minutes, seconds);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
TimeSpan Loom_LoRa::calculateNextEvent() {
    uint32_t secondsToWait = 0;
    if(heartbeatTimer_s > 0 && normWorkTimer_s > 0) // check if heartbeat was initialized
    {
        if(heartbeatTimer_s < normWorkTimer_s) {
            secondsToWait = heartbeatTimer_s;

            if(normWorkTimer_s - secondsToWait > 0)
                normWorkTimer_s = normWorkTimer_s - secondsToWait;
            else
                normWorkTimer_s = 5;

            heartbeatTimer_s = heartbeatInterval_s;
            heartbeatFlag = true;
        }
        else {
            secondsToWait = normWorkTimer_s;

            if (heartbeatTimer_s - secondsToWait > 0)
                heartbeatTimer_s = heartbeatTimer_s - secondsToWait;
            else
                heartbeatTimer_s = 5;

            normWorkTimer_s = normWorkInterval_s;
            heartbeatFlag = false;
        }

        if (secondsToWait < 5)
            secondsToWait = 5; // minimum wait time of 5 seconds for safety/stability.
    }
    return secondsToTimeSpan(secondsToWait);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::sendHeartbeat() {

    // this is 200 because it is safely within the P2P and LoRaWAN limits for maximum size.
    const uint16_t JSON_HEARTBEAT_BUFFER_SIZE = 200;

    StaticJsonDocument<JSON_HEARTBEAT_BUFFER_SIZE> heartbeatDoc;
    heartbeatDoc["type"] = "LoRa_heartbeat";
    heartbeatDoc.createNestedArray("contents");

    JsonObject objNestedId = heartbeatDoc.createNestedObject("id");
    objNestedId["name"] = manager->get_device_name();
    objNestedId["instance"] = manager->get_instance_num();

    heartbeatDoc["battery_voltage"] = Loom_Analog::getBatteryVoltage();

    if(hypnosPtr != nullptr) {
        char utcTimeStr[21];
        char localTimeStr[21];
        DateTime utcTime = hypnosPtr->getCurrentTime();
        DateTime localTime = hypnosPtr->getLocalTime(utcTime);
        hypnosPtr->dateTime_toString(utcTime, utcTimeStr);
        hypnosPtr->dateTime_toString(localTime, localTimeStr, true); // set third arg to true for local time format

        JsonObject objNestedTimestamp = heartbeatDoc.createNestedObject("timestamp");
        objNestedTimestamp["time_utc"] = utcTimeStr;
        objNestedTimestamp["time_local"] = localTimeStr;
    }

    return send(heartbeatDestAddress, heartbeatDoc.as<JsonObject>());
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::ensureHeartbeatHypnosAlarmsActive() {
    if(hypnosPtr == nullptr) {
        WARNING(F("Hypnos pointer not set - cannot ensure heartbeat alarm is active"));
        return;
    }
    
    bool setAlarm1 = false;
    bool setAlarm2 = false;
    uint32_t alarmOneTime = 0;
    uint32_t alarmTwoTime = 0;
    if(hypnosPtr->isAlarm1Cleared())
        setAlarm1 = true;
    else
        alarmOneTime = hypnosPtr->getAlarmDate(1).unixtime();
    if (hypnosPtr->isAlarm2Cleared())
        setAlarm2 = true;
    else
        alarmTwoTime = hypnosPtr->getAlarmDate(2).unixtime();
    uint32_t currentTime = hypnosPtr->getCurrentTime().unixtime();

    if(!setAlarm1)
    {
        if(alarmOneTime != 0)
            setAlarm1 = alarmOneTime <= currentTime;
        else
            WARNING(F("Alarm one time is zero - never pulled time from RTC"));
    }
    if(!setAlarm2)
    {
        if(alarmTwoTime != 0)
            setAlarm2 = alarmTwoTime <= currentTime;
        else
            WARNING(F("Alarm two time is zero - never pulled time from RTC"));
    }

    if(setAlarm1) {
        // check for overlap with an already set alarm two
        if(!setAlarm2 && currentTime + normWorkInterval_s > alarmTwoTime - 5
            && currentTime + normWorkInterval_s < alarmTwoTime + 5) {
                uint32_t remainingSecondsAlarmTwo = 0;
                if(alarmTwoTime - currentTime > 0)
                    remainingSecondsAlarmTwo = alarmTwoTime - currentTime;
                else
                    remainingSecondsAlarmTwo = 0;

                hypnosPtr->clearAlarms();

                TimeSpan timeSpanToSetWith = secondsToTimeSpan(normWorkInterval_s);
                hypnosPtr->setInterruptDuration(timeSpanToSetWith);
                timeSpanToSetWith = secondsToTimeSpan(heartbeatInterval_s + remainingSecondsAlarmTwo);
                hypnosPtr->setSecondAlarmInterruptDuration(timeSpanToSetWith);

                Serial.println("skipping heartbeat since normal work will overlap");

                return; // set both alarms already in this branch, so return.
        }
        else {
            hypnosPtr->clearAlarm1Register();
            TimeSpan timeSpanToSetWith = secondsToTimeSpan(normWorkInterval_s);
            hypnosPtr->setInterruptDuration(timeSpanToSetWith);
            Serial.println("Alarm set for normal work interval");
        }
    }

    alarmOneTime = hypnosPtr->getAlarmDate(1).unixtime(); // re-fetch alarm one time after potential reset above.

    if(setAlarm2) {
        // check for overlap with an already set alarm one
        if(!setAlarm1 && currentTime + heartbeatInterval_s > alarmOneTime - 5
            && currentTime + heartbeatInterval_s < alarmOneTime + 5) {
                // do nothing and skip heartbeat.
                Serial.println("Skipping heartbeat alarm to avoid conflict with normal work alarm");
        }
        else {
            hypnosPtr->clearAlarm2Register();
            TimeSpan timeSpanToSetWith = secondsToTimeSpan(heartbeatInterval_s);
            hypnosPtr->setSecondAlarmInterruptDuration(timeSpanToSetWith);
            Serial.println("Alarm set for heartbeat interval");
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LoRa::adjustHbFlagFromAlarms() {
    if(hypnosPtr == nullptr) {
        WARNING(F("Hypnos pointer not set - cannot adjust heartbeat flag based on alarms"));
        return;
    }

    uint8_t firedAlarmBM = hypnosPtr->getFiredAlarmsBM(); 

    hypnosPtr->clearAlarmFlags();

    Serial.print("Triggered Alarm Number: ");
    Serial.println(firedAlarmBM);

    if(firedAlarmBM == BM_ALARM_1 || firedAlarmBM == BM_BOTH) {
        ERROR("Either no alarms have fired or both alarms have fired - defaulting to alarm 1 behavior");
        setHeartbeatFlag(false);
        return;
    }

    if(firedAlarmBM == BM_ALARM_1) {
        setHeartbeatFlag(false); 
        Serial.println("Adjusted heartbeat flag from alarm 1");
        return;
    }
    else if (firedAlarmBM == BM_ALARM_2) {
        setHeartbeatFlag(true);
        Serial.println("Adjusted heartbeat flag from alarm 2");
        return;
    }

    return;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::send(const uint8_t destinationAddress) {
    return send(destinationAddress, manager->getDocument().as<JsonObject>());
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LoRa::send(const uint8_t destinationAddress, 
                            JsonObject json) {
    if (!moduleInitialized) {
        ERROR(F("Module not initialized!"));
        return false;
    }

    Serial.println(F("JSON About to be Sent:"));
    serializeJsonPretty(json, Serial);
    Serial.println();

    if (measureMsgPack(json) > MAX_MESSAGE_LENGTH) {
        return sendFragmentedPacket(json, destinationAddress);
    } else {
        return sendFullPacket(json, destinationAddress);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool Loom_LoRa::sendBatch(const uint8_t destinationAddress) {
    bool status = false;

    if (!moduleInitialized) {
        ERROR(F("Module not initialized!"));
        return false;
    }

    if (!batchSD) {
        ERROR(F("BatchSD module not set - cannot send batch"));
        return false;
    }

    if (!batchSD->shouldPublish()) {
        LOG(F("BatchSD not ready to publish"));
        return true;
    }

    File fileOutput = batchSD->getBatch();
    int batchSize = batchSD->getBatchSize();

    for (int i = 0; i < batchSize && fileOutput.available(); i++) {
        uint8_t packetBuf[2000];
        // read line from file into packetBuf
        int len = fileOutput.readBytesUntil('\n', packetBuf, 
                                            sizeof(packetBuf));

        if (!len) {
            WARNING(F("BatchSD data missing ending newline"));
            break;
        }

        // remove trailing carriage return if DOS line endings have been used
        if (packetBuf[len - 1] == '\r') {
            packetBuf[len - 1] = 0;
        }

        // deserialze packet into main document
        deserializeJson(manager->getDocument(), (const char *)packetBuf,
                        sizeof(packetBuf));

        status = send(destinationAddress);
        if (status) {
            LOGF("Successfully transmitted packet (%i/%i)", i+1, batchSize);
        } else {
            ERRORF("Failed to transmit packet (%i/%i)", i+1, batchSize);
        }

        delay(500);

        Serial.println();
    }

    fileOutput.close();
}

bool Loom_LoRa::receiveBatch(uint timeout, int* numberOfPackets) {
    uint8_t fromAddress;
    return receiveBatch(timeout, numberOfPackets, &fromAddress);
}

bool Loom_LoRa::receiveBatch(uint timeout, int* numberOfPackets, uint8_t *fromAddress) {
    bool status = receive(timeout, fromAddress, true);
    *numberOfPackets = expectedOutstandingPackets;
    return status;
}
