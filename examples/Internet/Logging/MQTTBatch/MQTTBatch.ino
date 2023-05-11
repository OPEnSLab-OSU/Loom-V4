/**
 * This is an example use case for Loomified wifi and MQTT to log data remotely in BATCHES
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Internet/Logging/Loom_MQTT/Loom_MQTT.h>

Manager manager("Device", 1);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

Loom_WIFI wifi(manager, CommunicationMode::CLIENT, SECRET_SSID, SECRET_PASS);
Loom_MQTT mqtt(manager, wifi.getClient(), SECRET_BROKER, SECRET_PORT, DATABASE, BROKER_USER, BROKER_PASS, PROJECT);

// Enables batch logging with a batch size of 15
Loom_BatchSD batchSD(hypnos, 15);

void setup() {

  // Start serial
  manager.beginSerial();

  // Enable the Hypnos
  hypnos.enable();

  // Set an instance of BatchSD on the wifi module
  wifi.setBatchSD(batchSD);

  // Initialize all modules
  manager.initialize();
}

void loop() {
  // Package data
  manager.package();

  manager.display_data();

  // Need to log to SD to store the batch data
  hypnos.logToSD();

  // Pass batch SD along to the MQTT module
  mqtt.publish(batchSD);

  // Wait 5 seconds
  manager.pause(5000);
}