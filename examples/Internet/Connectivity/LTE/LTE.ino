/**
 * This is an example use case for Loomified LTE
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

// Loom Modules
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>

Manager manager("Device", 1);

Loom_LTE lte(manager, NETWORK_NAME, NETWORK_USER, NETWORK_PASS);

void setup() {

  manager.beginSerial();
  manager.initialize();
}

void loop() {
  lte.verifyConnection();
  manager.pause(5000);
}