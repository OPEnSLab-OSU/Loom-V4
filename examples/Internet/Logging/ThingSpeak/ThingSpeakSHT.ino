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
#include "arduino_secrets.h"

#include <Loom_Manager.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>

#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>

//#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>

#include <Internet/Logging/Loom_ThingSpeak/Loom_ThingSpeak.h>

Manager manager("Device", 1);

Loom_WIFI wifi(manager, CommunicationMode::CLIENT, SECRET_SSID, SECRET_PASS);

//Loom_LTE lte(manager, NETWORK_NAME, NETWORK_USER, NETWORK_PASS);

// WiFi
Loom_ThingSpeak thingspeak(manager, wifi.getClient(), CHANNEL_ID, CLIENT_ID, BROKER_USER, BROKER_PASS);

// LTE
//Loom_ThingSpeak thingspeak(manager, lte.getClient(), CHANNEL_ID, CLIENT_ID, BROKER_USER, BROKER_PASS);

Loom_SHT31 sht(manager);

void setup() {
    manager.beginSerial();

    // Populates field 1 with the return value of exampleNoParam
    thingspeak.addFunction(1, sht.getTemperature);

    // Populates field 2 with the return value of exampleParam passing in 100 as the parameter
    thingspeak.addFunction(2, sht.getHumidity);

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