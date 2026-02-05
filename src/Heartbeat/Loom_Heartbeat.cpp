#include "Loom_Heartbeat.h"
#include "../Logger.h"


////////////////////////////////////////////////////////////////////////////////////////////////////// THEORETICALLY WORKS
Loom_Heartbeat::Loom_Heartbeat(const uint32_t pHeartbeatInterval, 
                        const uint32_t pNormalWorkInterval, 
                        Manager* managerInstance, 
                        Loom_Hypnos* hypnosInstance) {

    heartbeatInterval_s = pHeartbeatInterval;
    normWorkInterval_s = pNormalWorkInterval;

    heartbeatTimer_s = heartbeatInterval_s;
    normWorkTimer_s = normWorkInterval_s;

    managerPtr = managerInstance;
    hypnosPtr = hypnosInstance;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Heartbeat::sanitizeIntervals() {

    if(hypnosPtr != nullptr && heartbeatInterval_s < 60) {
        WARNING(F("Heartbeat interval too low for Hypnos, setting to minimum of 60 seconds"));
        heartbeatInterval_s = 60;
    }
    else if(heartbeatInterval_s < 5) {
        WARNING(F("Heartbeat interval too low, setting to minimum of 5 seconds"));
        heartbeatInterval_s = 5;
    }

    if(normWorkInterval_s < 5) {
        WARNING(F("Normal work interval too low, setting to minimum of 5 seconds"));
        normWorkInterval_s = 5;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
TimeSpan Loom_Heartbeat::secondsToTimeSpan(const uint32_t totalSeconds) {
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
TimeSpan Loom_Heartbeat::calculateNextEvent() {
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
void Loom_Heartbeat::flashLight() {
    pinMode(13, OUTPUT);

    digitalWrite(13, LOW);
    delay(1000);

    digitalWrite(13, HIGH);
    delay(1500); // 3 units (dash)

    digitalWrite(13, LOW);
    delay(500); 

    digitalWrite(13, HIGH);
    delay(500); // 1 unit (dot)

    digitalWrite(13, LOW);
    delay(1500);

    digitalWrite(13, HIGH);

    return;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Heartbeat::createJSONPayload(JsonDocument& heartbeatDoc) {
    heartbeatDoc.clear();

    heartbeatDoc["type"] = "heartbeat";
    heartbeatDoc.createNestedArray("contents");

    JsonObject objNestedId = heartbeatDoc.createNestedObject("id");
    objNestedId["name"] = managerPtr->get_device_name();
    objNestedId["instance"] = managerPtr->get_instance_num();

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
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Heartbeat::ensureHeartbeatHypnosAlarmsActive() {
    if(hypnosPtr == nullptr) {
        WARNING(F("Hypnos pointer not set - cannot ensure heartbeat alarm is active"));
        return;
    }

    Serial.println("checking the value of the fired alarms bitmask:  @@@@@@@@@@@@@@");
    Serial.println(hypnosPtr->getFiredAlarmsBM());

    // checks if the alarms fired.
    uint8_t firedAlarmsBitMask = hypnosPtr->getFiredAlarmsBM();
    bool setAlarm1 = firedAlarmsBitMask & BM_ALARM_1;
    bool setAlarm2 = firedAlarmsBitMask & BM_ALARM_2;
    hypnosPtr->clearFiredAlarmsBM();

    // check if alarm registers are cleared. Any cleared registers need to be re-set with the appropriate intervals.
    if(hypnosPtr->isAlarm1Cleared())
        setAlarm1 = true;
    if (hypnosPtr->isAlarm2Cleared())
        setAlarm2 = true;

    uint32_t alarmOneTime = 0;
    uint32_t alarmTwoTime = 0;

    if (!setAlarm1)
        alarmOneTime = hypnosPtr->getAlarmDate(1).unixtime();
    if (!setAlarm2)
        alarmTwoTime = hypnosPtr->getAlarmDate(2).unixtime();
    uint32_t currentTime = hypnosPtr->getCurrentTime().unixtime();


    // if both alarms need to be set, just set them both and return.
    if(setAlarm1 && setAlarm2) {
        hypnosPtr->clearAlarms();

        TimeSpan timeToSetWith = secondsToTimeSpan(normWorkInterval_s);
        hypnosPtr->setInterruptDuration(timeToSetWith);

        timeToSetWith = secondsToTimeSpan(heartbeatInterval_s);
        hypnosPtr->setSecondAlarmInterruptDuration(timeToSetWith);
        Serial.println("Both alarms reset");
        return;
    }
    else if(setAlarm1){
        // sets alarm one, and resets alarm two if alarm 1 would overlap with it.
        if (!setAlarm2 &&
            alarmTwoTime != 0 &&
            currentTime + normWorkInterval_s > alarmTwoTime - 5 &&
            currentTime + normWorkInterval_s < alarmTwoTime + 5) {

            uint32_t remainingSecondsAlarmTwo =
                (alarmTwoTime > currentTime) ? (alarmTwoTime - currentTime) : 0;

            hypnosPtr->clearAlarms();

            TimeSpan ts = secondsToTimeSpan(normWorkInterval_s);
            hypnosPtr->setInterruptDuration(ts);

            ts = secondsToTimeSpan(heartbeatInterval_s + remainingSecondsAlarmTwo);
            hypnosPtr->setSecondAlarmInterruptDuration(ts);

            ERROR("Skipping heartbeat since normal work will overlap");
            return;
        }
        // sets normal work alarm if no overlap detected.
        else {
            TimeSpan ts = secondsToTimeSpan(normWorkInterval_s);
            hypnosPtr->setInterruptDuration(ts);
            ERROR("Alarm set for normal work interval");
        }
    }
    else if(setAlarm2){
        // skips heartbeat alarm if it would overlap with normal work alarm.
        if (!setAlarm1 &&
            alarmOneTime != 0 &&
            currentTime + heartbeatInterval_s > alarmOneTime - 5 &&
            currentTime + heartbeatInterval_s < alarmOneTime + 5) {

            ERROR("Skipping heartbeat alarm to avoid conflict with normal work alarm");
            return;
        }
        // sets heartbeat alarm if no overlap detected.
        else {
            TimeSpan ts = secondsToTimeSpan(heartbeatInterval_s);
            hypnosPtr->setSecondAlarmInterruptDuration(ts);
            Serial.println("Alarm set for heartbeat interval");
        }
    }
    else {
        ERROR("No alarms were set - nothing to do");
        return;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Heartbeat::adjustHbFlagFromAlarms() {
    if(hypnosPtr == nullptr) {
        WARNING(F("Hypnos pointer not set - cannot adjust heartbeat flag based on alarms"));
        return;
    }

    bool alarm1Fired = hypnosPtr->alarm1Fired();
    bool alarm2Fired = hypnosPtr->alarm2Fired();

    ERROR("Triggered Alarm Number: ");
    if(alarm1Fired && alarm2Fired)
        Serial.println("3");
    else if(alarm2Fired)
        Serial.println("2");
    else if(alarm1Fired)
        Serial.println("1");
    else if(!alarm1Fired && !alarm2Fired)
        Serial.println("0");
    else
        Serial.println("Error determining alarm number");

    if(alarm1Fired && alarm2Fired) {
        Serial.println("Both alarms have fired - defaulting to alarm 1 behavior");
        setHeartbeatFlag(false);
        return;
    }

    if(alarm1Fired) {
        setHeartbeatFlag(false); 
        Serial.println("Adjusted heartbeat flag from alarm 1");
        return;
    }
    else if (alarm2Fired) {
        setHeartbeatFlag(true);
        Serial.println("Adjusted heartbeat flag from alarm 2");
        return;
    }

    ERROR("No alarms have fired - cannot adjust heartbeat flag");
    return;
}