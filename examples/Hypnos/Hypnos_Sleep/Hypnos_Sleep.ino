#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

Manager manager("Device", 1);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, false, false);

void wake() { hypnos.wakeup(); }

void setup() {
    manager.beginSerial(false);
    hypnos.setCompileTime(__DATE__, __TIME__);
    hypnos.enable();
    hypnos.registerInterrupt(wake);
}

void loop() {
    if (hypnos.scheduleWake(TimeSpan(10))) {
        hypnos.sleep();
    } else {
        delay(1000);
        hypnos.enable();
    }
}
