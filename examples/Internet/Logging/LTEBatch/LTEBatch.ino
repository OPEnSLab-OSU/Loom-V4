/**
 * This is an example use case for Loomified wifi and MQTT to log data remotely in BATCHES
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MQTT/Loom_MQTT.h>

Manager manager("Device", 1);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

Loom_LTE lte(manager, NETWORK_NAME, NETWORK_USER, NETWORK_PASS);
Loom_MQTT mqtt(manager, lte.getClient(), SECRET_BROKER, SECRET_PORT, DATABASE, BROKER_USER, BROKER_PASS);

// Enables batch logging with a batch size of 15
Loom_BatchSD batchSD(hypnos, 15);

// Called when the interrupt is triggered 
void isrTrigger(){
  hypnos.wakeup();
}

void setup() {

  // Start serial
  manager.beginSerial();

  // Sets the LTE board to use batch SD to only start when we actually need to publish data
  lte.enableBatch(batchSD);

  // Enable the Hypnos
  hypnos.enable();

  // Initialize all modules
  manager.initialize();

  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);

}

void loop() {
  // Package data
  manager.package();

  // Print the data
  manager.display_data();

  // Need to log to SD to store the batch data
  hypnos.logToSD();

  // Pass batch SD along to the MQTT module
  mqtt.publish(batchSD);
  
  // Set the RTC interrupt alarm to wake the device in 10 seconds
  hypnos.setInterruptDuration(TimeSpan(0, 0, 0, 10));

  // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
  hypnos.reattachRTCInterrupt();

  // Wait 5 seconds
  hypnos.sleep();
}