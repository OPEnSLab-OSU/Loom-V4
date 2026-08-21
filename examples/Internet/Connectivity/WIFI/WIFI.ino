/**
 * This is an example use case for Loomified wifi
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include "arduino_secrets.h"
#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>


Manager manager("Device", 1);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

Loom_WIFI wifi(manager, CommunicationMode::CLIENT, SECRET_SSID, SECRET_PASS);


void setup() 
{
  manager.beginSerial();

  hypnos.enable();

  manager.initialize();
}


void loop() 
{
  wifi.verifyConnection();

  manager.pause(5000);
}