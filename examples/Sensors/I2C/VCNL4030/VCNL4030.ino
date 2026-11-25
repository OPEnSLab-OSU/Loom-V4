/**
 * VCNL4030 example with Hypnos: proximity, lux and white channel every 10 sec, sleeping between.
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/I2C/Loom_VCNL4030/Loom_VCNL4030.h>

Manager manager("Device", 1);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);
Loom_VCNL4030 vcnl(manager);

void isrTrigger() { hypnos.wakeup(); }

void setup() {
    manager.beginSerial();

    hypnos.enable();

    manager.initialize();

    hypnos.registerInterrupt(isrTrigger);
}

void loop() {
    manager.measure();

    manager.package();

    manager.display_data();

    hypnos.logToSD();

    hypnos.setInterruptDuration(TimeSpan(0, 0, 0, 10));

    hypnos.reattachRTCInterrupt();

    hypnos.sleep();
}
