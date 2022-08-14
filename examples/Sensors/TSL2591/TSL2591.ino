/**
 * This is an example use case for setting a custom time on the Hypnos RTC
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/I2C/Loom_TSL2591/Loom_TSL2591.h>

Manager manager("Chime", 1);

// Create a new Hypnos object setting the version to determine the SD Chip select pin
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_2, TIME_ZONE::PST, false, false);
Loom_TSL2591 tsl2591(manager);


void setup() {

  // Start the serial interface
  manager.beginSerial();
  
  // Enable the hypnos rails
  hypnos.enable();
  manager.initialize();
}

void loop() {
  manager.measure();
  manager.package();

  manager.display_data();

  manager.pause(4000);
}