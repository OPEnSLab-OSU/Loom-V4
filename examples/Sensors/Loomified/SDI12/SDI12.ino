/**
 * This is an example use case for the SDI-12 implementation WITH the use of a manger to allow for maximum usability
 * A Non-Loomified version can be found in the "Standalone" section of the examples
 * 
 * This assumes a Hypnos board is in use as connection point if not you can remove the lines
 *  - pinMode(5, OUTPUT);
 *  - pinMode(6, OUTPUT);
 *  
 * This example scans the address space for devices and the requests data from the first device it finds
 */

#include <Loom_Manager.h>
#include <Loom_SDI12.h>


// Manager handles all loom simplicity 
Manager manager;

// The manager should be passed into each Loomified sensor to enable easy measure and packing functionality
Loom_SDI12 decagon(manager, 11);

void setup() {
  // Needs to be done for Hypnos Board
  pinMode(5, OUTPUT);       // Enable control of 3.3V rail
  pinMode(6, OUTPUT);       // Enable control of 5V rail

  manager.beginSerial();    // Open a serial connection and by default wait 20 seconds before printing if the Serial monitor isnt open.
  manager.initialize();     // Initialize in-use modules
  
}

void loop() {

  manager.measure();        // Pull the sensors for measurements
  manager.package();        // Package the data into JSON
  manager.printJSON();      // Print the current packaged json packet out

  delay(4000);              // Wait 4 seconds before pulling data again
}
