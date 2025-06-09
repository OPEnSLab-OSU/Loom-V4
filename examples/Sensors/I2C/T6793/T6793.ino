/**
 * This is an example use case for using the SEN55 Sensor
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Sensors/I2C/Loom_T6793/Loom_T6793.h>

Manager manager("Device", 1);
// Manager Reference, Whether or not we should measure particulate matter or nor
Loom_T6793 T6793(manager);

void setup() {

  // Start the serial interface
  manager.beginSerial();

  // Initialize the manager
  manager.initialize();
}

void loop() {
  // Measure the data from the sensors
  manager.measure();

  // Package the data into JSON
  manager.package();

  // Print the JSON document to the Serial monitor
  manager.display_data();

  // Wait for 5 seconds
  manager.pause(5000);
}
