/**
 * AS7265X Example code
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Sensors/I2C/Loom_AS7265X/Loom_AS7265X.h>

Manager manager("Device", 1);

// Reads the battery voltage
// Manger Instance, Address, Use Bulb, Gain, Mode, Integration Time
Loom_AS7265X as(manager, false, 0x49, false, 64, AS7265X_MEASUREMENT_MODE_6CHAN_ONE_SHOT, 50);

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
