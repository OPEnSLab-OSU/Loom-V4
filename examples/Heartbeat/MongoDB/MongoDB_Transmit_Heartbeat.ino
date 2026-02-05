/**
 * This is an example use case for LoRa communication
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Heartbeat/Loom_Heartbeat.h>

#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>

Manager manager("Device", 1);

Loom_LTE lte(manager);
Loom_MongoDB mqtt(manager, lte);

// heartbeat instantiation
uint32_t hbInterval_s = 15;
uint32_t normalInterval_s = 35;
Loom_Heartbeat heartbeat(hbInterval_s, normalInterval_s, &manager);

void setup() {
  manager.beginSerial();
  manager.initialize();
  //will need to pull mqtt creds from sd card
  mqtt.loadConfigFromJSON("name.csv");
}

void loop() {
    // logic to execute this loop
  if(heartbeat.getHeartbeatFlag())
  {
    char buffer[MAX_JSON_SIZE];
    Serial.println("Within Heartbeat Branch");
    JsonObject payload = heartbeat.createJSONPayload();
    serializeJson(payload, buffer);
    mqtt.publishMetadata(buffer);
  }
  else {
    // do work
    manager.package();
    Serial.println("Within Normal Work Branch");
  }

  int milliseconds = heartbeat.calculateNextEvent().totalseconds() * 1000;
  Serial.print("Pause for: ");
  Serial.print(String(milliseconds));
  Serial.println("");

  // Wait X seconds between transmits
  manager.pause(milliseconds);
}
