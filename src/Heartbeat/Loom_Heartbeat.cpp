#include "Loom_Heartbeat.h"
#include "../Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Heartbeat::Loom_Heartbeat(const uint32_t pHeartbeatInterval,
                               const uint32_t pNormalWorkInterval, Manager *managerInstance,
                               Loom_Hypnos *hypnosInstance) {

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

    if (hypnosPtr != nullptr && heartbeatInterval_s < 60) {
        WARNING(F("Heartbeat interval too low for Hypnos, setting to minimum of 60 seconds"));
        heartbeatInterval_s = 60;
    } else if (heartbeatInterval_s < 15) {
        WARNING(F("Heartbeat interval too low, setting to minimum of 15 seconds"));
        heartbeatInterval_s = 15;
    }

    if (normWorkInterval_s < 15) {
        WARNING(F("Normal work interval too low, setting to minimum of 15 seconds"));
        normWorkInterval_s = 15;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
TimeSpan Loom_Heartbeat::secondsToTimeSpan(const uint32_t totalSeconds) {
    // Build from d/h/m/s:
    uint16_t rem = 0;
    uint16_t days = totalSeconds / 86400;
    rem = totalSeconds % 86400;
    uint8_t hours = rem / 3600;
    rem = rem % 3600;
    uint8_t minutes = rem / 60;
    uint8_t seconds = rem % 60;

    return TimeSpan(days, hours, minutes, seconds);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
TimeSpan Loom_Heartbeat::calculateNextEvent() {
    /**
     *  logic for calculating the time to rest, and resetting the timers to the state they should be
     * in after waking up. The main idea is to compare the timers and set the pause to be the
     * smaller of the two. Then, we adjust the timers based on which one we set pause with, and if
     * they are close enough we round them both to 5 seconds to avoid any issues with very short
     * sleep times.
     */
    uint32_t secondsToWait = 0;
    if (heartbeatTimer_s < normWorkTimer_s) {
        secondsToWait = heartbeatTimer_s; // grab time to delay program execution for

        // ensure the larger timer does not get set to less than 10 seconds to avoid issues with
        // very short sleep times.
        if (normWorkTimer_s - secondsToWait > 10)
            normWorkTimer_s =
                normWorkTimer_s -
                secondsToWait; // adjust larger timer (normal work) by smaller timer amount
        else
            normWorkTimer_s = 10;

        heartbeatTimer_s = heartbeatInterval_s; // reset smaller timer (heartbeat) to be interval
        heartbeatFlag = true;
    } else {
        secondsToWait = normWorkTimer_s;

        // ensure the larger timer does not get set to less than 10 seconds to avoid issues with
        // very short sleep times.
        if (heartbeatTimer_s - secondsToWait > 10)
            heartbeatTimer_s =
                heartbeatTimer_s -
                secondsToWait; // adjust larger timer (heartbeat) by smaller timer amount
        else
            heartbeatTimer_s = 10;

        normWorkTimer_s = normWorkInterval_s; // reset smaller timer (normal work) to be interval
        heartbeatFlag = false;
    }

    if (secondsToWait < 10)
        secondsToWait = 10; // minimum wait time of 10 seconds for safety/stability.
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
void Loom_Heartbeat::createJSONPayload(JsonDocument &heartbeatDoc) {
    heartbeatDoc.clear();

    heartbeatDoc["type"] = "heartbeat";
    heartbeatDoc.createNestedArray("contents");

    JsonObject objNestedId = heartbeatDoc.createNestedObject("id");
    objNestedId["name"] = managerPtr->get_device_name();
    objNestedId["instance"] = managerPtr->get_instance_num();

    heartbeatDoc["battery_voltage"] = Loom_Analog::getBatteryVoltage();

    if (hypnosPtr != nullptr) {
        char utcTimeStr[21];
        char localTimeStr[21];
        DateTime utcTime = hypnosPtr->getCurrentTime();
        DateTime localTime = hypnosPtr->getLocalTime(utcTime);
        hypnosPtr->dateTime_toString(utcTime, utcTimeStr);
        hypnosPtr->dateTime_toString(localTime, localTimeStr,
                                     true); // set third arg to true for local time format

        JsonObject objNestedTimestamp = heartbeatDoc.createNestedObject("timestamp");
        objNestedTimestamp["time_utc"] = utcTimeStr;
        objNestedTimestamp["time_local"] = localTimeStr;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Heartbeat::ensureHeartbeatHypnosAlarmsActive() {
    if (hypnosPtr == nullptr) {
        WARNING(F("Hypnos pointer not set - cannot ensure heartbeat alarm is active"));
        return;
    }

    // checks if the alarms fired.
    ALARM_BITMASKS firedAlarmsBitMask = hypnosPtr->getFiredAlarmsBM();
    bool setAlarm1 =
        (firedAlarmsBitMask & ALARM_BITMASKS::BM_ALARM_1) != ALARM_BITMASKS::BM_NONE;
    bool setAlarm2 =
        (firedAlarmsBitMask & ALARM_BITMASKS::BM_ALARM_2) != ALARM_BITMASKS::BM_NONE;
    // check if alarm registers are cleared. Any cleared registers need to be re-set with the
    // appropriate intervals.
    if (firedAlarmsBitMask == ALARM_BITMASKS::BM_NONE) {
        setAlarm1 = true;
        setAlarm2 = true;
    }
    hypnosPtr->clearFiredAlarmsBM();

    uint32_t alarmOneTime = 0;
    uint32_t alarmTwoTime = 0;

    if (!setAlarm1)
        alarmOneTime = hypnosPtr->getAlarmDate(1).unixtime();
    if (!setAlarm2)
        alarmTwoTime = hypnosPtr->getAlarmDate(2).unixtime();
    uint32_t currentTime = hypnosPtr->getCurrentTime().unixtime();

    // if both alarms need to be set, just set them both and return.
    if (setAlarm1 && setAlarm2) {
        hypnosPtr->clearAlarms();

        TimeSpan timeToSetWith = secondsToTimeSpan(normWorkInterval_s);
        hypnosPtr->setInterruptDuration(timeToSetWith);

        timeToSetWith = secondsToTimeSpan(heartbeatInterval_s);
        hypnosPtr->setSecondAlarmInterruptDuration(timeToSetWith);
        LOG("Both alarms set");
        return;
    } else if (setAlarm1) {
        // sets alarm one, and resets alarm two if alarm 1 would overlap with it.
        if (!setAlarm2 && alarmTwoTime != 0 &&
            currentTime + normWorkInterval_s > alarmTwoTime - 10 &&
            currentTime + normWorkInterval_s < alarmTwoTime + 10) {

            uint32_t remainingSecondsAlarmTwo =
                (alarmTwoTime > currentTime) ? (alarmTwoTime - currentTime) : 0;

            hypnosPtr->clearAlarms();

            TimeSpan ts = secondsToTimeSpan(normWorkInterval_s);
            hypnosPtr->setInterruptDuration(ts);

            ts = secondsToTimeSpan(heartbeatInterval_s + remainingSecondsAlarmTwo);
            hypnosPtr->setSecondAlarmInterruptDuration(ts);

            LOG("Skipping heartbeat since normal work will overlap");
            return;
        }
        // sets normal work alarm if no overlap detected.
        else {
            TimeSpan ts = secondsToTimeSpan(normWorkInterval_s);
            hypnosPtr->setInterruptDuration(ts);
            LOG("Alarm 1 set for normal work interval");
        }
    } else if (setAlarm2) {
        // skips heartbeat alarm if it would overlap with normal work alarm.
        if (!setAlarm1 && alarmOneTime != 0 &&
            currentTime + heartbeatInterval_s > alarmOneTime - 10 &&
            currentTime + heartbeatInterval_s < alarmOneTime + 10) {

            LOG("Skipping heartbeat alarm to avoid conflict with normal work alarm");
            return;
        }
        // sets heartbeat alarm if no overlap detected.
        else {
            TimeSpan ts = secondsToTimeSpan(heartbeatInterval_s);
            hypnosPtr->setSecondAlarmInterruptDuration(ts);
            LOG("Alarm 2 set for heartbeat interval");
        }
    } else {
        ERROR("No alarms were set - nothing to do");
        return;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Heartbeat::adjustHbFlagFromAlarms() {
    if (hypnosPtr == nullptr) {
        WARNING(F("Hypnos pointer not set - cannot adjust heartbeat flag based on alarms"));
        return;
    }

    bool alarm1Fired = hypnosPtr->alarm1Fired();
    bool alarm2Fired = hypnosPtr->alarm2Fired();

    if (alarm1Fired && alarm2Fired) {
        LOG("Both alarms have fired - defaulting to alarm 1 behavior");
        setHeartbeatFlag(false);
        return;
    }

    if (alarm1Fired) {
        setHeartbeatFlag(false);
        LOG("Adjusted heartbeat flag to false from alarm 1 triggering");
        return;
    } else if (alarm2Fired) {
        setHeartbeatFlag(true);
        LOG("Adjusted heartbeat flag to true from alarm 2 triggering");
        return;
    }

    ERROR("No alarms have fired - cannot adjust heartbeat flag");
    setHeartbeatFlag(false); // safe fallback: default to normal work
    return;
}
