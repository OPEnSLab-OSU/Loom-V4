/**
 * LoRa Time-Division Multiple Access (TDMA) Scheduled Node
 * 
 * Sends packets during this device's assigned TDMA time slot.
 * Lower 4 bits of address determine slot (0x21 = slot 1, 0x22 = slot 2, etc).
 * Change DEVICE_ADDRESS for each node to use different slots.
 */

#include <Loom_Manager.h>
#include <Radio/Loom_LoRa/Loom_LoRa.h>

#define DEVICE_ADDRESS 0x21

Manager manager("Device");
Loom_LoRa lora(manager, DEVICE_ADDRESS, 23, 3, 3, 200);

void setup() {
  manager.beginSerial();
  manager.initialize();
}

void loop() {
  manager.package();
  manager.display_data();
  
  // Send during this device's TDMA time slot
  lora.sendScheduled();
}
