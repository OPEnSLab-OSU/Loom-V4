#include <Loom_Manager.h>

#include <Sensors/I2C/Loom_WCMCU75/Loom_WCMCU75.h>

Manager manager("Device", 1);

Loom_WCMCU75 wcmcu75(manager);

void setup() {
    manager.beginSerial();

    manager.initialize();
}

void loop() {
    manager.measure();

    manager.package();

    manager.display_data();

    manager.pause(5000);
}