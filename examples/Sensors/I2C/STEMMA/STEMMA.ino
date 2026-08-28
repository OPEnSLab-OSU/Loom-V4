/**
 * STEMMA Example code
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Sensors/I2C/Loom_STEMMA/Loom_STEMMA.h>


Manager manager("Device", 1);

Loom_STEMMA stemma(manager);    // Reads the battery voltage


void setup() 
{
  // Start the serial interface
  manager.beginSerial();

  // Initialize the manager
  manager.initialize();
}


void loop() 
{
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
