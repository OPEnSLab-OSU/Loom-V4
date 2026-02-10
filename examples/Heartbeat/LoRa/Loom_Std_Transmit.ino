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

  heartbeat.sanitizeIntervals();
}

void loop() {
    // logic to execute this loop
  if(heartbeat.getHeartbeatFlag())
  {
    Serial.println("Within Heartbeat Branch");

    heartbeat.flashLight();
    
    const uint16_t JSON_HEARTBEAT_BUFFER_SIZE = 300;
    StaticJsonDocument<JSON_HEARTBEAT_BUFFER_SIZE> payload;
    heartbeat.createJSONPayload(payload);

    // append any additional information you wish to send over heartbeat
    payload["exampleField"] = "example";
    JsonObject nestedJson = payload.createNestedObject("NestedName");
    nestedJson["nestExample1"] = "nest1";
    nestedJson["nestExample2"] = "nest2";

    lora.send(0, payload.as<JsonObject>());
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
