
/**
 * This is an example use case for using the SEN55 Sensor
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Sensors/I2C/Loom_SEN66/Loom_SEN66.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>


Manager manager("Device", 1);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);
// Manager Reference, Whether or not we should measure particulate matter or nor
Loom_SEN66 sen66(manager, true);


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
  // Measure the data from the sensors
  manager.measure();

  // Package the data into JSON
  manager.package();

  // Print the JSON document to the Serial monitor
  manager.display_data();

  // Wait for 5 seconds
  manager.pause(5000);
}
