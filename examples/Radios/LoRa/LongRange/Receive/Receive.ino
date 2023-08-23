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

  // Wait up to 15 seconds for a message, this is cause it takes AT LEAST 13 seconds in the air to reach the receiver 
  if(lora.receive(15000)){
    manager.display_data();
  }
}