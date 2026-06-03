/**
 * Manager-owned MemPool JSON document demo.
 *
 * Shows that Manager::getDocument() is backed by the Manager-owned MemPool,
 * prints pool stats, packages mock data, and displays the serialized JSON.
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

Manager manager("Device", 1);
bool demoRan = false;

void setup() {
    manager.beginSerial();

    Serial.println(F("\n--- Manager MemPool JSON document demo start ---"));
    Serial.print(F("Manager JSON capacity: "));
    Serial.println(manager.getDocument().capacity());

    manager.printPoolStats();

    manager.package();
    manager.addData("MockSensor", "Temperature_C", 21.5f);
    manager.addData("MockSensor", "Humidity_RH", 45.0f);
    manager.addData("MockSensor", "Battery_V", 3.85f);

    manager.display_data();
    manager.printPoolStats();
    manager.dumpActivePoolLeases();

    Serial.println(F("--- Manager MemPool JSON document demo complete ---"));
    demoRan = true;
}

void loop() {
    static unsigned long lastPrint = 0;
    if (Serial && (millis() - lastPrint >= 2000)) {
        lastPrint = millis();
        Serial.println(demoRan ? F("[loop] demo complete, board still running")
                               : F("[loop] waiting for demo"));
    }
}