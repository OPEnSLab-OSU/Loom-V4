/**
 * Digital pin reading and packaging example using the Manager
 * Pass in a variable number of arguments to the construct to read which pins you want
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Sensors/Loom_Digital/Loom_Digital.h>

Manager manager("Device", 1);

// Sets the pinMode of pin 12 and 11 to INPUT_PULLUP and then reads values from the pins
Loom_Digital digital(manager, INPUT_PULLUP, 12, 11);

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
