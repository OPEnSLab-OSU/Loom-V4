/**
 * K30 Example code
 * HARDWARE DEPRECATED - This sensor is no longer supported on the current
 * board profile. Use at your own risk.
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Sensors/I2C/Loom_K30/Loom_K30.h>


Manager manager("Device", 1);

// Manger Instance, Address Enable Warmup, Value Multiplier
Loom_K30 k30(manager, false, 0x68, true, 1);


void setup() 
{
  // Start the serial interface and wait for the user to open the serial monitor
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


void loop() 
{
  // put your main code here, to run repeatedly:
}