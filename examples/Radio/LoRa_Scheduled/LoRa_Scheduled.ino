/**
 * An optional UTC schedule for one group of LoRa nodes. All nodes need synchronized UTC RTCs.
 * Address 0x21 is group 2, slot 1; its hub is 0x20. Settings below are seconds, not minutes.
 * Preparation begins before the slot, then radio transmission starts only inside the slot.
 * Choose a send guard large enough for the complete packet/retry budget measured on your radio.
 */
#include <Loom_Manager.h>
#include <Logger.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Radio/Loom_LoRa/Loom_LoRa.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>

Manager manager("ScheduledNode", 1);
// SD is required for lora_schedule.json and the sample copy used after failed radio delivery.
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, false, true);
loomLoRa::Schedule schedule; // This borrowed object must outlive the radio below.
Loom_LoRa lora(manager);
Loom_Analog battery(manager);
const uint8_t NODE_ADDRESS = 0x21, HUB_ADDRESS = 0x20;
const uint32_t SEND_GUARD_SECONDS = 60;
bool configured = false;
bool prepared = false;
uint32_t lastSentCycle = UINT32_MAX;
void rtcWake() { hypnos.wakeup(); }

////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
    manager.beginSerial(false);
    hypnos.setCompileTime(__DATE__, __TIME__);
    hypnos.enable();
    manager.initialize();
    lora.setAddress(NODE_ADDRESS);
    char *settings = hypnos.readFile("lora_schedule.json");
    configured = lora.loadScheduleFromJSON(settings, schedule);
    free(settings); // readFile owns an allocation; the loader only borrowed it during parsing.
    configured = configured && hypnos.registerInterrupt(rtcWake);
    if (!configured) {
        ERROR(F("Missing/invalid LoRa schedule; transmissions stay paused."));
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
    DateTime now;
    if (!configured || !hypnos.tryGetCurrentTime(now)) {
        manager.pause(5000);
        return;
    }
    uint32_t utc = now.unixtime();
    const uint32_t wakeWait = lora.secondsUntilTransmitWake(utc);
    if (wakeWait == UINT32_MAX) {
        ERROR(F("This node address has no configured slot."));
        manager.pause(5000);
        return;
    }
    if (wakeWait > 0) {
        prepared = false;
        if (hypnos.scheduleWake(TimeSpan(static_cast<int32_t>(wakeWait)))) {
            hypnos.sleep(false);
        } else {
            manager.pause(5000);
        }
        return;
    }
    if (!prepared) {
        manager.measure();
        manager.package();
        if (!hypnos.logToSD()) {
            WARNING(F("Sample could not be saved to SD; it remains only in this RAM packet."));
        }
        prepared = true;
    }
    if (!hypnos.tryGetCurrentTime(now)) {
        manager.pause(5000);
        return;
    }
    utc = now.unixtime(); // Measurement/warm-up may have taken time; check the slot again.
    const uint32_t cycle = schedule.cycleNumber(utc);
    if (cycle != lastSentCycle &&
        schedule.secondsLeftInSlot(NODE_ADDRESS, utc) >= SEND_GUARD_SECONDS) {
        if (lora.sendScheduled(HUB_ADDRESS, utc)) {
            lastSentCycle = cycle;
        } else {
            WARNING(F("Scheduled send failed; retry while slot has time. Check SD save result."));
        }
    }
    manager.pause(1000); // Preparation/slot polling only; the long off-slot interval sleeps above.
}
////////////////////////////////////////////////////////////////////////////////////////////////////
