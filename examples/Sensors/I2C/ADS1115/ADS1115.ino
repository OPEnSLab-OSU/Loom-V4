/**
 * ADS1115 Example code showing utilizing custom function calculators to store modified additional data
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>
#include <Sensors/I2C/Loom_ADS1115/Loom_ADS1115.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>


Manager manager("Device", 1);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

// Manger Instance, Enable Analog, Enable DIfferential, Gain
Loom_ADS1115 ads(manager);


void setup() 
{
  // Start the serial interface and wait for the user to open the serial monitor
  manager.beginSerial();
  
  // Enable hypnos for RTC
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