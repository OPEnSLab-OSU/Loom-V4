#include <cassert>
#include "../../src/Sensors/SPI/Loom_ADS1232/ADS1232_Lib_Fixed.cpp"
#include "../../src/Sensors/SPI/Loom_ADS1232/Loom_ADS1232.cpp"

static uint32_t now;
static bool ready = true;
static const uint8_t conversion[] = {0x80, 0x00, 0x2A};
static size_t conversionByte;

uint32_t millis() { return now; }
void delay(uint32_t ms) { now += ms; }
void delayMicroseconds(unsigned int) {}
void pinMode(int, int) {}
int digitalRead(int) { return ready ? LOW : HIGH; }
void digitalWrite(int, int) {}
uint8_t shiftIn(int, int, int) { return conversion[conversionByte++]; }

int main() {
    Manager manager;
    Loom_ADS1232 sensor(manager, 1, 0, 1.0f);
    sensor.initialize();
    sensor.measure();
    sensor.package();
    assert(manager.document["ADS1232"]["weight"].as<float>() == 42.0f);

    ready = false;
    sensor.measure();
    sensor.package();
    const JsonObject sample = manager.document["ADS1232"].as<JsonObject>();
    assert(sample.containsKey("weight") && sample["weight"].isNull());
    assert(now == 1500);

    ready = true;
    conversionByte = 0;
    sensor.measure();
    sensor.package();
    assert(sample["weight"].as<float>() == 42.0f);
}
