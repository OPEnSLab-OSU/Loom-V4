/**
 * This is a lab example combine LoRa communication and 4G connectivity
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

#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Radio/Loom_LoRa/Loom_LoRa.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>

Manager manager("Device", 0);

// Do we want to use the instance number as the LoRa address
Loom_LoRa loRa(manager, 0);
Loom_LTE lte(manager, NETWORK_NAME, NETWORK_USER, NETWORK_PASS);
Loom_MongoDB mqtt(manager, lte.getClient(), SECRET_BROKER, SECRET_PORT, DATABASE, BROKER_USER, BROKER_PASS);

void setup() {

  manager.beginSerial();
  manager.initialize();
}

void loop() {

  // Wait 5 seconds for a message
  if(loRa.receive(5000)){

    // If a message was received display the JSON document and transmit it over MQTT
    manager.display_data();
    mqtt.publish();
  }
  
  // Wait 5 seconds
  manager.pause(5000);
}