/**
 * This is an example use case for LoRa communication
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Hardware/Loom_BatchSD/Loom_BatchSD.h>
#include <Radio/Loom_LoRa/Loom_LoRa.h>

Manager manager("Device", 1);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

// Do we want to use the instance number as the LoRa address
Loom_LoRa lora(manager);

// Create a batch size of 15
Loom_BatchSD batch(hypnos, 15);

void setup() {
  manager.beginSerial();

  // Set a reference to the batchSD object
  lora.setBatchSD(batch);

  hypnos.enable();

  manager.initialize();
}

void loop() {
  manager.package();
  manager.display_data();

  // Log the hypnos to the SD card
  hypnos.logToSD();

  // Send the current JSON document to address 0
  lora.sendBatch(0);

  // Wait 5 seconds between transmits
  manager.pause(5000);
}