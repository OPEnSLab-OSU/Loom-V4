#include "Loom_Heartbeat.h"
#include "Loom_Manager.h"
#include "Hardware/Loom_Hypnos/Loom_Hypnos.h"
#include "Sensors/Loom_Analog/Loom_Analog.h"
#include "Logger.h"
#include "Utilities/Loom_HeartbeatPayload.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Heartbeat::begin(uint32_t nowUtc) { return manager != nullptr && timers.begin(nowUtc); }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Heartbeat::Event Loom_Heartbeat::poll(uint32_t nowUtc) { return timers.poll(nowUtc); }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t Loom_Heartbeat::secondsUntilNext(uint32_t nowUtc) const {
    return timers.secondsUntilNext(nowUtc);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Heartbeat::createJSONPayload(JsonDocument &output) {
    FUNCTION_START;
    if (manager == nullptr || &output == &manager->getDocument() ||
        (hypnos != nullptr && !hypnos->isRTCInitialized())) {
        return false;
    }
    if (!loomHeartbeat::buildStatusPayload(output, manager->get_device_name(),
                                           manager->get_instance_num())) {
        return false;
    }
    const float volts = Loom_Analog::getBatteryVoltage();
    if (isfinite(volts) && volts > 0) {
        output["battery_voltage"] = volts;
    } else {
        output["battery_voltage"] = nullptr;
    }
    if (hypnos != nullptr) {
        DateTime now;
        if (!hypnos->tryGetCurrentTime(now)) {
            output.clear();
            return false;
        }
        char utc[21];
        hypnos->dateTime_toString(now, utc);
        output["timestamp"]["time_utc"] = utc; // Mutable text is copied into the document.
    }
    FUNCTION_END;
    return loomJsonIsComplete(output);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Heartbeat::flashLight() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(300);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    LOOM_FEED_WATCHDOG();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
