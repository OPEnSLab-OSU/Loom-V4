/**
 * This is an example use case for the SDI-12 implementation WITH the use of a manger to allow for maximum usability
 * This example scans the address space for devices and the requests data from the first device it finds
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Sensors/SDI12/Loom_SDI12/Loom_SDI12.h>


// Manager handles all loom simplicity 
Manager manager("Device", 1);

Loom_SDI12 sdi(manager, 11);

void setup() {

  // Start the serial interface
  manager.beginSerial();

  // Initialize the manager
  manager.initialize();
}

void loop() {
  // put your main code here, to run repeatedly:

  // Measure the data from the sensors
  manager.measure();

  // Package the data into JSON
  manager.package();

  // Print the JSON document to the Serial monitor
  manager.display_data();

  // Wait for 5 seconds
  manager.pause(5000);
}
