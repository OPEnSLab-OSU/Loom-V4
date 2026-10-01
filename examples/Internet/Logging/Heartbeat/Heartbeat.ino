/**
 * Two UTC timers, one wake alarm: measure every 15 minutes, send a heartbeat every hour.
 * Set USE_MONGO_HEARTBEAT to 1 for LTE/Mongo; otherwise this uses LoRa link acknowledgments.
 * Heartbeats have their own JSON document, so they never overwrite measurements or increment
 * the measurement packet number. Delivery failures are reported; this demo does not queue them.
 */
#include <Loom_Manager.h>
#include <Logger.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Heartbeat/Loom_Heartbeat.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#ifndef USE_MONGO_HEARTBEAT
#define USE_MONGO_HEARTBEAT 0
#endif
#if USE_MONGO_HEARTBEAT
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
#else
#include <Radio/Loom_LoRa/Loom_LoRa.h>
#endif

Manager manager("HeartbeatDemo", 1);
// Both transports save samples; the Mongo option also reads broker credentials from SD.
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, false, true);
Loom_Analog battery(manager);
#if USE_MONGO_HEARTBEAT
Loom_LTE lte(manager, "hologram", "", "");
Loom_MongoDB mqtt(manager, lte); // Supply mqtt_creds.json on SD, never paste credentials here.
#else
Loom_LoRa lora(manager);
const uint8_t HUB_ADDRESS = 0;
#endif
Loom_Heartbeat heartbeat(3600, 900, &manager, &hypnos);
StaticJsonDocument<512> heartbeatPacket;
bool timersReady = false;
void rtcWake() { hypnos.wakeup(); }

////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
    manager.beginSerial(false);
    hypnos.setCompileTime(__DATE__, __TIME__);
    hypnos.enable();
    manager.initialize();
#if USE_MONGO_HEARTBEAT
    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));
#endif
    DateTime utc;
    timersReady = hypnos.registerInterrupt(rtcWake) && hypnos.tryGetCurrentTime(utc) &&
                  heartbeat.begin(utc.unixtime());
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
    DateTime utc;
    if (!timersReady || !hypnos.tryGetCurrentTime(utc)) {
        manager.pause(5000); // Never sleep using an unknown deadline.
        return;
    }
    const Loom_Heartbeat::Event event = heartbeat.poll(utc.unixtime());
    if (event == Loom_Heartbeat::Event::Work) {
        manager.measure();
        manager.package();
        const bool saved = hypnos.logToSD();
        if (!saved) {
            WARNING(F("Measurement save failed; this demo has only the current RAM sample."));
        }
#if USE_MONGO_HEARTBEAT
        if (!mqtt.publish()) {
#else
        if (!lora.send(HUB_ADDRESS)) {
#endif
            WARNING(saved ? F("Measurement delivery failed; the SD copy remains available.")
                          : F("Measurement delivery failed without a saved SD copy."));
        }
    } else if (event == Loom_Heartbeat::Event::Heartbeat &&
               heartbeat.createJSONPayload(heartbeatPacket)) {
        heartbeatPacket["status"] = "awake"; // Add your own small health fields here.
        if (!loomJsonIsComplete(heartbeatPacket)) {
            WARNING(F("Heartbeat custom fields exceed its fixed JSON capacity."));
        } else {
#if USE_MONGO_HEARTBEAT
            const bool sent = mqtt.publishHeartbeat(heartbeatPacket);
#else
            const bool sent = lora.sendHeartbeat(HUB_ADDRESS, heartbeatPacket);
#endif
            if (sent) {
                heartbeat.flashLight();
            } else {
                WARNING(F("Heartbeat delivery failed."));
            }
        }
    }
    if (!hypnos.tryGetCurrentTime(utc)) {
        manager.pause(5000);
        return;
    }
    const uint32_t wait = heartbeat.secondsUntilNext(utc.unixtime());
    if (wait == 0) {
        return; // Work and heartbeat may share a deadline; handle the other event next loop.
    }
    if (hypnos.scheduleWake(TimeSpan(static_cast<int32_t>(wait)))) {
        hypnos.sleep(false);
    } else {
        manager.pause(5000);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
