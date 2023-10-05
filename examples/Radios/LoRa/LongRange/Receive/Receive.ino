/**
 * This is an example use case for long range LoRa receive
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Radio/Loom_LoRa/Loom_LoRa.h>

Manager manager("Device", 0);

// Create a new lora instance using the instance number as the address
Loom_LoRa lora(manager, LORA_RANGE::LONG);

void setup() {
  manager.beginSerial();
  manager.initialize();
}

void loop() {

  // Wait up to 60 seconds for a message, this is cause it takes a long time in the air to reach the receiver 
  if(lora.receive(60000)){
    manager.display_data();
  }
}