/**
 * EZO Dissolved Oxygen Sensor Example
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Sensors/I2C/Loom_EZODO/Loom_EZODO.h>

Manager manager("Device", 1);

// Manager Instance,      Address, useMux
Loom_EZODO ezoDO(manager, 0x61, false);

void setup() {

  // Start the serial interface
  manager.beginSerial();

  // Initialize the manager
  manager.initialize();
  
}

void loop() {
  // Measure and package the data from the sensors
  manager.measure();
  
  // Package the data into JSON
  manager.package();

  // Print the current JSON document to the Serial monitor
  manager.display_data();  

  // Wait for 2 seconds
  manager.pause(2000);  
  
}
