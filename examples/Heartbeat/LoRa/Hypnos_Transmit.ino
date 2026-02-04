/**
 * This is an example use case for LoRa communication
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

#include <Radio/Loom_LoRa/Loom_LoRa.h>

#include <Heartbeat/Loom_Heartbeat.h>

Manager manager("Device", 1);

// Create a new Hypnos object setting the version to determine the SD Chip select pin, and starting without the SD card functionality
//Loom_Hypnos(Manager& man, HYPNOS_VERSION version, TIME_ZONE zone, bool use_custom_time = false, bool useSD = true)
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, false, false);

// Do we want to use the instance number as the LoRa address
Loom_LoRa lora(manager);

// heartbeat instantiation
uint32_t hbInterval_s = 60;
uint32_t normalInterval_s = 35;
Loom_Heartbeat heartbeat(hbInterval_s, normalInterval_s, &manager, &hypnos);

// Called when the interrupt is triggered 
void isrTrigger(){
  hypnos.wakeup();
}

void setup() {
  hypnos.enable();
  
  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);

  manager.beginSerial();
  manager.initialize();

  heartbeat.sanitizeIntervals();
  hypnos.clearAlarms();
}

void loop() {
  heartbeat.adjustHbFlagFromAlarms();

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

  // set up both alarms for the heartbeat mode using the specified intervals earlier.
  heartbeat.ensureHeartbeatHypnosAlarmsActive();

  // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
  hypnos.reattachRTCInterrupt();

  Serial.flush(); // Forces all serial log messages in buffer to print now.
  
  // Put the device into a deep sleep, operation HALTS here until the interrupt is triggered
  hypnos.sleep();

  delay(100); // CRITICAL: Without this delay, output and operation does not align with expectations.
}
