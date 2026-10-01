#include <assert.h>
#include <string>
#include "../../src/Sensors/Loom_Analog/Loom_Analog.cpp"

void analogReadResolution(uint8_t) {}
void pinMode(int, int) {}
void delayMicroseconds(unsigned int) {}
int analogRead(int) { return 2048; }

std::string packet(Manager &manager) {
    std::string result;
    serializeJson(manager.document, result);
    return result;
}
int main() {
    Manager manager;
    Loom_Analog sensor(manager, 14, 16);
    sensor.measure();
    sensor.package();
    const std::string original = packet(manager);
    assert(original.find("\"A0\"") < original.find("\"A0_MV\""));
    assert(original.find("\"A2_MV\"") < original.find("\"Vbat\""));
    assert(manager.document["Analog"].size() == 6);

    sensor.setOutputColumns(false, true);
    manager.document.clear();
    sensor.package();
    assert(manager.document["Analog"].size() == 3);
    assert(manager.document["Analog"].containsKey("Vbat_MV"));
    assert(!manager.document["Analog"].containsKey("Vbat"));
    assert(sensor.getAnalog(14) == 2048 && sensor.getMV(14) > 0);

    sensor.setOutputColumns(true, false);
    manager.document.clear();
    sensor.package();
    assert(manager.document["Analog"].size() == 3);
    assert(!manager.document["Analog"].containsKey("A0_MV"));

    sensor.setOutputColumns(false, false);
    manager.document.clear();
    sensor.package();
    assert(manager.document["Analog"].size() == 0);
    sensor.setOutputColumns(true, true);
    manager.document.clear();
    sensor.package();
    assert(packet(manager) == original);
    assert(Loom_Analog::getBatteryVoltage(9, 12, 3.3f, 2, 0) == 0);
    assert(Loom_Analog::getBatteryVoltage(9, 12, 3.3f, 2, 10, 0) == 0);
}
