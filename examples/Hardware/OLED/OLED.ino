/**
 * OLED Example code showing how to display the current packets
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Hardware/Loom_OLED/Loom_OLED.h>

Manager manager("Device", 1);

// Manger Instance, Enable Analog, Enable DIfferential, Gain
Loom_OLED oled(manager);

void setup() {

  // Start the serial interface and wait for the user to open the serial monitor
  manager.beginSerial();

  // Initialize the manager
  manager.initialize();

}

void loop() {
  // Measure the data from the sensors
  manager.measure();

  // Package the data into JSON
  manager.package();

  // Print the JSON document to the Serial monitor and Display it on the OLED
  manager.display_data();

  // Wait for 2 seconds
  manager.pause(2000);

}