/**
 * A radio-only "I'm alive" node. It does not package sensors or increment measurement packets.
 * For fresh UTC/battery/custom fields, create a separate packet with Loom_Heartbeat and call
 * lora.sendHeartbeat(HUB_ADDRESS, packet). This basic demo never invents a timestamp.
 */
#include <Loom_Manager.h>
#include <Logger.h>
#include <Radio/Loom_LoRa/Loom_LoRa.h>

Manager manager("AliveNode", 1);
Loom_LoRa lora(manager, 23, 3, 200, Loom_LoRa::Mode::HeartbeatOnly);
const uint8_t HUB_ADDRESS = 0;
const uint32_t HEARTBEAT_INTERVAL_MS = 60000;
uint32_t lastHeartbeat = 0;
bool firstHeartbeat = true;

////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
    manager.beginSerial(false);
    manager.initialize();
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
    const uint32_t now = millis();
    if (!firstHeartbeat && static_cast<uint32_t>(now - lastHeartbeat) < HEARTBEAT_INTERVAL_MS) {
        return; // Rollover-safe demo timer; use Hypnos UTC alarms for a sleeping deployment.
    }
    firstHeartbeat = false;
    lastHeartbeat = now;
    if (!lora.send(HUB_ADDRESS)) {
        WARNING(F("Heartbeat was not acknowledged; retry on the next heartbeat interval."));
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
