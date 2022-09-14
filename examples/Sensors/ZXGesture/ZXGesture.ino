/**
 * Temperature Readings using teh MAX31865 sensor
 * Pass in a variable number of arguments to the construct to designate the number of samples and which pin you're using
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Sensors/I2C/Loom_ZXGesture/Loom_ZXGesture.h>


Manager manager("Device", 1);

// Gets gestures from sensor
Loom_ZXGesture zx(manager);

void setup() {

  // Start the serial interface
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
