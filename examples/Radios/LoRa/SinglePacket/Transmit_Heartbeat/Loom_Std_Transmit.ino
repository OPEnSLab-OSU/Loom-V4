/**
 * This is an example use case for LoRa communication
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Radio/Loom_LoRa/Loom_LoRa.h>

Manager manager("Device", 1);

// Do we want to use the instance number as the LoRa address
Loom_LoRa lora(manager);

void setup() {
  manager.beginSerial();
  manager.initialize();

  // initialize heartbeat operations
  uint8_t destAddr = 0;
  uint32_t hbInterval_s = 15;
  uint32_t normalInterval_s = 35;
  lora.heartbeatInit(destAddr, hbInterval_s, normalInterval_s);
}

void loop() {
  manager.package();
  manager.display_data();

    // logic to execute this loop
  if(lora.getHeartbeatFlag())
  {
    Serial.println("Within Heartbeat Branch");
    lora.sendHeartbeat();
  }
  else {
    // do work
    Serial.println("Within Normal Work Branch");
    lora.send(0);
  }

  int seconds = lora.hbNextEvent().totalseconds() * 1000;
  Serial.print("Sleep for: ");
  Serial.print(String(seconds));
  Serial.println("");

  // Wait X seconds between transmits
  manager.pause(seconds);
}