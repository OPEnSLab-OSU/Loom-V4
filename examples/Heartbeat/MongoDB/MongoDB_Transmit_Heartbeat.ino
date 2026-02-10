/**
 * This is an example use case for LoRa communication
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Heartbeat/Loom_Heartbeat.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>

Manager manager("Device", 1);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true);
Loom_LTE lte(manager, "hologram", "", "");
Loom_MongoDB mqtt(manager, lte);

// heartbeat instantiation
uint32_t hbInterval_s = 15;
uint32_t normalInterval_s = 35;
Loom_Heartbeat heartbeat(hbInterval_s, normalInterval_s, &manager);

void isrTrigger()
{
  hypnos.wakeup();
}

void setup() {
  manager.beginSerial();
    // Both power rails should be on when awake
  hypnos.setWakeConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);

  // Only the 5V rail should be on during sleep
  hypnos.setSleepConfiguration(POWERRAIL_CONFIG::PR_3V_OFF_5V_ON);

  // Enable the hypnos rails
  hypnos.enable();

  //Time Sync Using LTE 
  hypnos.setNetworkInterface(&lte);

  //will need to pull mqtt creds from sd card
  mqtt.loadConfigFromJSON(hypnos.readFile("heartbeat.json"));

  manager.initialize();

  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);

  hypnos.networkTimeUpdate();

}

void loop() {
    // logic to execute this loop
  if(heartbeat.getHeartbeatFlag())
  {
    char buffer[MAX_JSON_SIZE];
    Serial.println("Within Heartbeat Branch");
    const uint16_t JSON_HEARTBEAT_BUFFER_SIZE = 200;
    StaticJsonDocument<JSON_HEARTBEAT_BUFFER_SIZE> basePayload;
    heartbeat.createJSONPayload(basePayload);
    serializeJson(basePayload, buffer);
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
