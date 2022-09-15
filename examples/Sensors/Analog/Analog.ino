/**
 * Analog pin reading and packaging example using the Manger
 * Pass in a variable number of arguments to the construct to read which pins you want
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Sensors/Loom_Analog/Loom_Analog.h>


Manager manager("Device", 1);

// Reads the battery voltage
Loom_Analog analog(manager);

// Read the battery voltage and A2
//Loom_Analog analog(manager, A2);

// Read the battery voltage, A2 and A4
//Loom_Analog analog(manager, A2, A4);

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
