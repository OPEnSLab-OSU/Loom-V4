/**
 * This is an example use case for Loomified LTE
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

// Loom Modules

#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>

//#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

Manager manager("Device", 1);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

/* Network interfaces */

// WiFi
Loom_WIFI wifi(manager, CommunicationMode::CLIENT, "OSU_Access", "");

// LTE
//Loom_LTE lte(manager, "hologram", "", "");

void setup() {

  manager.beginSerial();

  hypnos.enable();

  // Using WiFi
  hypnos.setNetworkInterface(&wifi);

  //Using LTE
  //hypnos.setNetworkInterface(&lte);
  

  manager.initialize();

  hypnos.networkTimeUpdate();
}

void loop() {
  manager.pause(5000);
  hypnos.networkTimeUpdate();
}