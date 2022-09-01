/**
 * This is an example use case for Freewave radio communication
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Radio/Loom_Freewave/Loom_Freewave.h>

Manager manager("Device", 1);

// Do we want to use the instance number as the LoRa address
Loom_Freewave fw(manager, 1);

void setup() {

  manager.beginSerial();
  manager.initialize();
}

void loop() {
  manager.package();
  manager.display_data();

  // Send the current JSON document to address 1
  fw.send(1);

  // Wait 5 seconds between transmits
  manager.pause(5000);
}
