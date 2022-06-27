/**
 * This is an example use case for the Hypnos board's SD logging functionality
 * This allows the user to log sensor and debug data to an SD card inserted into the Hypnos
 */

#include <Loom_Hypnos.h>
#include <Loom_Manager.h>

Manager manager("Chime", 1);

// Create a new Hypnos object setting the version to determine the SD Chip select pin
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_2);


void setup() {

   // Start and wait for the user to open the Serial monitor
  Serial.begin(115200);
  while(!Serial);
  
  // Enable the hypnos rails
  hypnos.enable();

  manager.addData("Test", "Test1", 31);
  manager.addData("Test", "Test2", 34);

  manager.printJSON();
  
  hypnos.logToSD();
  hypnos.logToSD();
}

void loop() {
  
}
