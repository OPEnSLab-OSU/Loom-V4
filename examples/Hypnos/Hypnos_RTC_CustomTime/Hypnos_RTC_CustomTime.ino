#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

Manager manager("Device", 1);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, false, false);

void setup() {
    manager.beginSerial();
    hypnos.enable();
    if (Serial) {
        Serial.println(F("Select Newline in Serial Monitor, then enter the current UTC time."));
        hypnos.set_custom_time();
    }
}

void loop() {
    DateTime now;
    if (Serial && hypnos.tryGetCurrentTime(now)) {
        char utc[21];
        hypnos.dateTime_toString(now, utc);
        Serial.println(utc);
    }
    delay(4000);
}
