/**
 * This is an example use case for remotely logging data to ThingSpeak
 * 
 * There is a maximum of 8 fields that can be populated with data
 * 
 * Supported function signatures for retrieving data are as follows:
 *      float name()
 *      float name(int param)
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

//#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>

#include <Internet/Logging/Loom_ThingSpeak/Loom_ThingSpeak.h>

Manager manager("Device", 1);

Loom_WIFI wifi(manager, CommunicationMode::CLIENT, SECRET_SSID, SECRET_PASS);

//Loom_LTE lte(manager, NETWORK_NAME, NETWORK_USER, NETWORK_PASS);

// WiFi
Loom_ThingSpeak thingspeak(manager, wifi.getClient(), CHANNEL_ID, CLIENT_ID, BROKER_USER, BROKER_PASS);

// LTE
//Loom_ThingSpeak mqtt(manager, lte.getClient(), CHANNEL_ID, CLIENT_ID, BROKER_USER, BROKER_PASS);

float exampleNoParam() {
    return 45.6;
}

float exampleParam(int param) {
    return 75 + param;
}

void setup() {
    manager.beginSerial();

    // Populates field 1 with the return value of exampleNoParam
    thingspeak.addFunction(1, exampleNoParam);

    // Populates field 2 with the return value of exampleParam passing in 100 as the parameter
    thingspeak.addFunction(2, exampleParam, 100);

    /*
        For Loom sensors you just need to pass in the "get" function for example:
                mqtt.addFunction(1, sht.getTemperature());
    */

    manager.initialize();
}

void loop() {
    /* Measure, package display, publish */
    manager.measure();
    manager.package();

    manager.display_data();

    thingspeak.publish();

    manager.pause(5000);
}