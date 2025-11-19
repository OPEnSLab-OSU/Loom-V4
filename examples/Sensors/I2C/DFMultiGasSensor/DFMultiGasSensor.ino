/**
 * Example for the DF Robot Multi Gas Sensor
 */
#include <Loom_Manager.h>
#include <Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.h>


// If the sensor is freezing on init try disconnecting the power and re-connecting it
Manager manager("Device", 1);

// MANAGER, I2C ADDRESS, INIT RETRY LIMIT, USE MUX
Loom_DFMultiGasSensor gas(manager, 0x77, 10, false);

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