/**
 * This is an example use case for LoRa communication
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Radio/Loom_LoRa/Loom_LoRa_Sketch.h>
#include <Logger.h>

Manager manager("Device", 0);

// Create a new lora instance using the instance number as the address
Loom_LoRa_Sketch lora(manager);

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
