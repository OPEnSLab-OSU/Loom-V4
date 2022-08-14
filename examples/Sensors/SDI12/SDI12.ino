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

  manager.beginSerial();    // Open a serial connection and by default wait 20 seconds before printing if the Serial monitor isnt open.
  manager.initialize();     // Initialize in-use modules
  
}

void loop() {

  manager.measure();        // Pull the sensors for measurements
  manager.package();        // Package the data into JSON
  manager.display_data();      // Print the current packaged json packet out

  manager.pause(4000)              // Wait 4 seconds before pulling data again
}
