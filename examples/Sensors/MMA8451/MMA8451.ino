/**
 * MMA8451 Example code
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Sensors/I2C/Loom_MMA8451/Loom_MMA8451.h>

Manager manager("Device", 1);

// Reads the battery voltage
// Manger Instance, Address, Range
Loom_MMA8451 mma(manager, false, 0x1D, MMA8451_RANGE_2_G);

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
