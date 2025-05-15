#include <Loom_Manager.h>
#include <Sensors/I2C/Loom_MultiGasSensor/Loom_MultiGasSensor.h>


// If the sensor is freezing on init try disconnecting the power and re-connecting it
Manager manager("Device", 1);
Loom_MultiGasSensor gasSensor(manager, 0x77, false);

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