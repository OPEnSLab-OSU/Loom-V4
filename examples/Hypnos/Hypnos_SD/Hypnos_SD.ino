/**
 * This is an example use case for the Hypnos board's SD logging functionality
 * This allows the user to log sensor and debug data to an SD card inserted into the Hypnos
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

Manager manager("Device", 1);

// Create a new Hypnos object setting the version to determine the SD Chip select pin
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_2, TIME_ZONE::PST);


void setup() {

  manager.beginSerial();
  
  // Enable the hypnos rails
  hypnos.enable();

  manager.addData("Test", "Test1", 31);
  manager.addData("Test", "Test2", 34);

  manager.display_data();
  
  hypnos.logToSD();
  hypnos.logToSD();
}

void loop() {
  
}
