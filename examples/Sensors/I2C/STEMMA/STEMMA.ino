/**
 * STEMMA Example code
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>
#include <Sensors/I2C/Loom_STEMMA/Loom_STEMMA.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>


Manager manager("Device", 1);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

// Reads the battery voltage
Loom_STEMMA stemma(manager);


void setup() 
{
  // Start the serial interface
  manager.beginSerial();

  // Enable hypnos for RTC
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
