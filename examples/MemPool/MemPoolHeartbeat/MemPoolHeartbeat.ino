/**
 * MemPool-backed heartbeat LED example.
 *
 * This sketch uses Loom_Heartbeat without Hypnos, so it does not require an RTC.
 * When the heartbeat interval fires, it creates the heartbeat JSON payload in the
 * Manager memory pool and flashes the built-in LED.
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Heartbeat/Loom_Heartbeat.h>
#include <MemPoolJson.hpp>

Manager manager("HeartbeatLED", 1);

const uint32_t HEARTBEAT_INTERVAL_S = 15;
const uint32_t NORMAL_WORK_INTERVAL_S = 30;
const size_t HEARTBEAT_JSON_SIZE = 300;

Loom_Heartbeat heartbeat(HEARTBEAT_INTERVAL_S, NORMAL_WORK_INTERVAL_S, &manager);

void setup() {
    manager.beginSerial();
    manager.initialize();

    pinMode(LED_BUILTIN, OUTPUT);
    heartbeat.sanitizeIntervals();
    heartbeat.setHeartbeatFlag(true);
}

void loop() {
    if (heartbeat.getHeartbeatFlag()) {
        Serial.println(F("Heartbeat branch"));

        LoomJsonDocument payload(HEARTBEAT_JSON_SIZE, MemPoolJsonAllocator(&manager.getPool()));
        if (payload.capacity() == 0) {
            Serial.println(F("Failed to allocate heartbeat JSON document from memory pool"));
            manager.printPoolStats();
            manager.pause(5000);
            return;
        }

        heartbeat.createJSONPayload(payload);
        payload["exampleField"] = "mempool-heartbeat";

        Serial.print(F("Heartbeat payload: "));
        serializeJson(payload, Serial);
        Serial.println();

        heartbeat.flashLight();
    } else {
        Serial.println(F("Normal work branch"));
        digitalWrite(LED_BUILTIN, LOW);
    }

    uint32_t milliseconds = heartbeat.calculateNextEvent().totalseconds() * 1000UL;
    Serial.print(F("Pause for: "));
    Serial.print(milliseconds);
    Serial.println(F(" ms"));

    manager.pause(milliseconds);
}
