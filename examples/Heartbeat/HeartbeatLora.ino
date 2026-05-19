#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Radio/Loom_LoRa/Loom_LoRa.h>
#include <Heartbeat/Heartbeat.h>

Manager manager("Device", 1);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

Loom_Analog analog(manager);

Loom_LoRa lora(manager);

// Set normal work interval is an integer corresponding to how often 
// a normal packet is transmitted instead of a heartbeat packet
// Ex: Send a normal packet every 4 packets
// CRUCIAL: Pass in manager, hypnos, and lora objects
Loom_Heartbeat heartbeat(4, &manager, &hypnos, &lora);

// Called when the interrupt is triggered 
void isrTrigger(){
  hypnos.wakeup();
}

void setup() {

  // Start the serial interface
  manager.beginSerial();

  // Enable the hypnos rails
  hypnos.enable();

  // Called after enable
  manager.initialize();

  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);
}

void loop() {

  // Set the RTC interrupt alarm to wake the device in 10 seconds, at the top to schedule next interrupt asap
  hypnos.setInterruptDuration(TimeSpan(0, 0, 0, 10));

  // Measure and package data
  manager.measure();
  manager.package();
  
  // Print the current JSON packet
  manager.display_data();            

  // Log the data to the SD card              
  hypnos.logToSD();

  if (heartbeat.getHeartbeatFlag()) {
    heartbeat.makeHeartbeat();
    heartbeat.addData("FieldName", "DataName", "Data");
    heartbeat.transmit(0);
  }
  else {
    lora.send(0);
  }

  // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
  hypnos.reattachRTCInterrupt();
  
  // Put the device into a deep sleep, operation HALTS here until the interrupt is triggered
  hypnos.sleep();
}