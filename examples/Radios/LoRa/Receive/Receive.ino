/**
 * This is an example use case for LoRa communication
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Radio/Loom_LoRa/Loom_LoRa.h>

Manager manager("Device", 0);

// Do we want to use the instance number as the LoRa address
Loom_LoRa lora(manager);

void setup() {

  manager.beginSerial();
  manager.initialize();
}

void loop() {

  // Wait 5 seconds for a message
  if(lora.receive(5000)){
    manager.display_data();
  }
}