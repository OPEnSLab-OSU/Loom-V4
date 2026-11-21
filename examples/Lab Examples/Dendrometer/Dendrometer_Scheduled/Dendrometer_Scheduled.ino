#include <Loom_Manager.h>
#include <Logger.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Radio/Loom_LoRa/Loom_LoRaJournal.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Sensors/SPI/Loom_AS5311/AS5311.h>

// Set this address on each board. Use 0x20 for the hub and 0x21 through 0x24 for nodes.
// Fit SD cards and set each RTC to UTC with Hypnos_RTC_CustomTime before uploading.
#ifndef DENDROMETER_ADDRESS
#define DENDROMETER_ADDRESS 0x21
#endif
constexpr uint8_t HUB = 0x20;
static_assert(DENDROMETER_ADDRESS >= HUB && DENDROMETER_ADDRESS <= 0x24,
              "Choose the hub address or one of the four node addresses.");

Manager manager("Dendrometer", DENDROMETER_ADDRESS);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, false);
Loom_LoRa radio(manager);
loomLoRa::Schedule schedule;

enum { Displacement, Temperature, Humidity, Battery, SampleFields };
float reading[SampleFields];
Loom_LoRaJournal journal(radio, hypnos, sizeof(reading));

#if DENDROMETER_ADDRESS != 0x20
Loom_SHT31 sht(manager);
AS5311 magnet(A3, A5, A4);

void wake() { hypnos.wakeup(); }

void measureReading() {
    manager.measure();
    reading[Displacement] = magnet.measureDisplacement();
    reading[Temperature] = sht.getTemperature();
    reading[Humidity] = sht.getHumidity();
    reading[Battery] = Loom_Analog::getBatteryVoltage();
}
#endif

void setup() {
    Logger::getInstance()->setDebugOutput(false);
    manager.beginSerial(false);
    hypnos.enable();
    manager.initialize();
    schedule.configure(45, 5, 300, 0); // Each node gets a 45-second slot every 5 minutes.
    radio.setSchedule(schedule);
#if DENDROMETER_ADDRESS != 0x20
    hypnos.registerInterrupt(wake);
#endif
}

void loop() {
#if DENDROMETER_ADDRESS == 0x20
    uint8_t source;
    uint32_t sampled;
    if (journal.pollHub(source, sampled, reading) == Loom_LoRaJournal::ReceiveResult::Saved) {
        Serial.print(source, HEX);
        Serial.print(',');
        Serial.print(sampled);
        for (float value : reading) {
            Serial.print(',');
            Serial.print(value, 3);
        }
        Serial.println();
    }
#else
    journal.sampleAndSend(HUB, reading, measureReading);
#endif
}
