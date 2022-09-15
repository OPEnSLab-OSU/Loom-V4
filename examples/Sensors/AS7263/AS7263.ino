/**
 * AS7263 Example code
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Sensors/I2C/Loom_AS7263/Loom_AS7263.h>

Manager manager("Device", 1);

// Reads the battery voltage
// Manger Instance,      useMux Address, Gain, Mode, Integration Time
Loom_AwS7262 as(manager, false, 0x49,    1,    3,    50);

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
