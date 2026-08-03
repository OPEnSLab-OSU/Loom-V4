/**
 * NOVASDS Example Code
 *
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Sensors/Serial/Loom_NOVASDS/Loom_NOVASDS.h>

Manager manager("Device", 1);

// Manager Instance, Serial interface
Loom_NOVASDS011 nova(manager, &Serial1);

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
