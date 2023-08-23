/**
 * This is an example use case for long range LoRa transmit
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Radio/Loom_LoRa/Loom_LoRa.h>

Manager manager("Device", 1);

// Do we want to use the instance number as the LoRa address
Loom_LoRa lora(manager, LORA_RANGE::LONG);

void setup() {
  manager.beginSerial();
  manager.initialize();
}

void loop() {
  manager.package();
  manager.display_data();

  // Send the current JSON document to address 0
  lora.send(0);

  // Wait 20 seconds between transmits
  manager.pause(20000);
}