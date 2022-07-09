/**
 * This is an example use case for Freewave radio communication
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Radio/Loom_Freewave/Loom_Freewave.h>

Manager manager("Device", 1);

// Do we want to use the instance number as the LoRa address
Loom_Freewave fw(manager);

void setup() {

  manager.beginSerial();
  manager.initialize();
}

void loop() {

  // Wait 5 seconds for a message
  if(fw.receive(5000)){
    manager.display_data();
  }
}