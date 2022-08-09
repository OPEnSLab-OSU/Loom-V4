/**
 * ADS1115 Example code showing utilizing custom function calculators to store modified additional data
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>
#include <cmath>

#include <Sensors/I2C/Loom_ADS1115/Loom_ADS1115.h>

Manager manager("Device", 1);

// Reads the battery voltage
Loom_ADS1115 ads(manager);

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