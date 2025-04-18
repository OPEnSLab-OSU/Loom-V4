/**
 * Example for how to use the Remote manager functionality of Loom
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
*/
#include <Loom_Manager.h>

#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Internet/Communication/Loom_RemoteManager/Loom_RemoteManager.h>

Manager manager("Device", 1);

// Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);
// Loom_LTE lte(manager, "hologram", "", "");
Loom_WIFI wifi(manager, CommunicationMode::CLIENT, "meow :3", "awawawawa");

Loom_RemoteManager remoteManager(manager, wifi, "cas-mosquitto.biossys.oregonstate.edu", 1883, "User", "password");

void setup(){
    /* Standard startup procedure */
    manager.beginSerial();
    // hypnos.enable();
    manager.initialize();
}

void loop(){
    /* Wake up collect data and the go to sleep and loop every 5 seconds */
    manager.power_up();
    manager.measure();
    manager.package();
    // manager.power_down();
    manager.pause(5000);
}
