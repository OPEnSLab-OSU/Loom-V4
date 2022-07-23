/**
 * Digital pin reading and packaging example using the Manager
 * Pass in a variable number of arguments to the construct to read which pins you want
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Sensors/Loom_Digital/Loom_Digital.h>

Manager manager("Device", 1);

// Reads the battery voltage
Loom_Digital digital(manager, 12, 11);

void setup() {

  // Start the serial interface
  manager.beginSerial();

  // Initialize the manager
  manager.initialize();

  // Measure the data from the sensors
  manager.measure();

  // Package the data into JSON
  manager.package();

  // Print the JSON document to the Serial monitor
  manager.display_data();

}

void loop() {
  // put your main code here, to run repeatedly:

}
