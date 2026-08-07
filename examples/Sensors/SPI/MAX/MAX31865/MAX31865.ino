/**
 * Temperature Readings using teh MAX31865 sensor
 * Pass in a variable number of arguments to the construct to designate the number of samples and which pin you're using
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

#include <Sensors/SPI/Loom_MAX318XX/Loom_MAX31865.h>


Manager manager("Device", 1);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

Loom_MAX31865 max65(manager);   // Reads the temperature


void setup() 
{
  // Start the serial interface
  manager.beginSerial();

  hypnos.enable();

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
