/**
 * Temperature Readings using teh MAX31856 sensor
 * Pass in a variable number of arguments to the construct to designate the number of samples and which chip pins you're using for SPI
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Sensors/SPI/Loom_MAX318XX/Loom_MAX31856.h>

Manager manager("Device", 1);

// create sensor
// default parameters:
// Manager& man, int samples = 1, int chip_select = 10, int mosi = -1, int miso = -1, int sclk = -1, TEMP_UNIT unit = CELCIUS
Loom_MAX31856 maxthermo(manager);

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
