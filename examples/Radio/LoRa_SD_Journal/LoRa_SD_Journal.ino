#include <Loom_Manager.h>
#include <Logger.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Radio/Loom_LoRa/Loom_LoRaJournal.h>

// Set this address on each board. Use 0x20 for the hub and 0x21 or 0x22 for nodes.
// Fit SD cards and set each RTC to UTC with Hypnos_RTC_CustomTime before uploading.
#ifndef LORA_JOURNAL_ADDRESS
#define LORA_JOURNAL_ADDRESS 0x21
#endif
constexpr uint8_t ADDRESS = LORA_JOURNAL_ADDRESS;
constexpr uint8_t HUB = 0x20;
static_assert(ADDRESS >= HUB && ADDRESS <= 0x22, "Choose the hub or a node address.");

Manager manager("LoRaJournal", ADDRESS);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, false);
Loom_LoRa radio(manager);
loomLoRa::Schedule schedule;
uint16_t reading;
Loom_LoRaJournal journal(radio, hypnos, sizeof(reading));

void wake() { hypnos.wakeup(); }
void measureReading() { reading = analogRead(A0); }

void setup() {
    Logger::getInstance()->setDebugOutput(false);
    manager.beginSerial(false);
    hypnos.enable();
    manager.initialize();
    schedule.configure(30, 3, 300, 0); // Each node gets a 30-second slot every 5 minutes.
    radio.setSchedule(schedule);
    if (ADDRESS != HUB) {
        hypnos.registerInterrupt(wake);
    }
}

void loop() {
    if (ADDRESS != HUB) {
        journal.sampleAndSend(HUB, &reading, measureReading);
        return;
    }
    uint8_t source;
    uint32_t sampled;
    if (journal.pollHub(source, sampled, &reading) == Loom_LoRaJournal::ReceiveResult::Saved) {
        Serial.print(source, HEX);
        Serial.print(',');
        Serial.print(sampled);
        Serial.print(',');
        Serial.println(reading);
    }
}
