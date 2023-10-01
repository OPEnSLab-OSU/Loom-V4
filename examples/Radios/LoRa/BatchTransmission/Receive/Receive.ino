/**
 * This is an example use case for LoRa communication
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Radio/Loom_LoRa/Loom_LoRa.h>

Manager manager("Device", 0);

// Create a new lora instance using the instance number as the address
Loom_LoRa lora(manager);

int packetNumber = 0;

void setup() {
  manager.beginSerial();
  manager.initialize();
}

void loop() {
  /* Handle each individual packet being received from the batch transmit */
  do{
    if(lora.receiveBatch(5000, &packetNumber)){
      manager.display_data();
    }
  }while(packetNumber > 0)
}