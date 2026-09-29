/**
 * This is an example use case for using the MPU gyroscope in conjunction with max
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

#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Internet/Communication/Loom_Max/Loom_Max.h>
#include <Sensors/I2C/Loom_MPU6050/Loom_MPU6050.h>


Manager manager("Device", 1);

Loom_WIFI wifi(manager, CommunicationMode::CLIENT, SECRET_SSID, SECRET_PASS);
Loom_Max maxMsp(manager, wifi);
Loom_MPU6050 mpu(manager);


void setup() {

  manager.beginSerial();
  manager.initialize();
}

void loop() {
  manager.measure();
  manager.package();
  manager.display_data();

  // Send and Recieve data from Max
  maxMsp.publish();
  //maxMsp.subscribe();
  manager.pause(50);
}