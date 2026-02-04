/**
 * This is an example use case for LoRa communication
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Radio/Loom_LoRa/Loom_LoRa.h>

#include <Heartbeat/Loom_Heartbeat.h>

Manager manager("Device", 1);

// Do we want to use the instance number as the LoRa address
Loom_LoRa lora(manager);

// heartbeat instantiation
uint32_t hbInterval_s = 15;
uint32_t normalInterval_s = 35;
Loom_Heartbeat heartbeat(hbInterval_s, normalInterval_s, &manager);

void setup() {
  manager.beginSerial();
  manager.initialize();
}

void loop() {
    // logic to execute this loop
  if(heartbeat.getHeartbeatFlag())
  {
    Serial.println("Within Heartbeat Branch");
    
    // this is 200 because it is safely within the P2P and LoRaWAN limits for maximum size.
    const uint16_t JSON_HEARTBEAT_BUFFER_SIZE = 200;
    StaticJsonDocument<JSON_HEARTBEAT_BUFFER_SIZE> basePayload;
    heartbeat.createJSONPayload(basePayload);
    lora.send(0, basePayload.as<JsonObject>());
  }
  else {
    // do work
    manager.package();
    Serial.println("Within Normal Work Branch");
    lora.send(0);
  }

  int milliseconds = heartbeat.calculateNextEvent().totalseconds() * 1000;
  Serial.print("Pause for: ");
  Serial.print(String(milliseconds));
  Serial.println("");

  // Wait X seconds between transmits
  manager.pause(milliseconds);
}
