/**
 * This is an example use case for Loomified wifi and MQTT to log data remotely in BATCHES
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#if __has_include("arduino_secrets.h")
#include "arduino_secrets.h"
#endif

#ifndef SECRET_SSID
#define SECRET_SSID ""
#endif
#ifndef SECRET_PASS
#define SECRET_PASS ""
#endif
#ifndef NETWORK_APN
#define NETWORK_APN ""
#endif
#ifndef NETWORK_NAME
#define NETWORK_NAME ""
#endif
#ifndef NETWORK_USER
#define NETWORK_USER ""
#endif
#ifndef NETWORK_PASS
#define NETWORK_PASS ""
#endif
#ifndef SECRET_BROKER
#define SECRET_BROKER ""
#endif
#ifndef SECRET_PORT
#define SECRET_PORT 0
#endif
#ifndef DATABASE
#define DATABASE ""
#endif
#ifndef BROKER_USER
#define BROKER_USER ""
#endif
#ifndef BROKER_PASS
#define BROKER_PASS ""
#endif
#ifndef PROJECT
#define PROJECT ""
#endif
#ifndef CHANNEL_ID
#define CHANNEL_ID 0
#endif
#ifndef CLIENT_ID
#define CLIENT_ID ""
#endif

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>

Manager manager("Device", 1);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

Loom_WIFI wifi(manager, CommunicationMode::CLIENT, SECRET_SSID, SECRET_PASS);
Loom_MongoDB mqtt(manager, wifi.getClient(), SECRET_BROKER, SECRET_PORT, DATABASE, BROKER_USER, BROKER_PASS, PROJECT);

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