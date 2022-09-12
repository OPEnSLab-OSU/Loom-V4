/**
 * AS7262 Example code
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Sensors/I2C/Loom_AS7262/Loom_AS7262.h>

Manager manager("Device", 1);

// Manger Instance,     Address, Gain, Mode, Integration Time
Loom_AS7262 as(manager, 0x49,    1,    3,    50);

void setup() {

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

void loop() {
  // put your main code here, to run repeatedly:

}