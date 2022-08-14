/**
 * This is an example use case for Loomified ethernet
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Internet/Connectivity/Loom_Ethernet/Loom_Ethernet.h>

Manager manager("Device", 1);

JsonArray mac;
mac.add("AA", "AA". "AA" "AA", "AA", "AA");
Loom_Ethernet ethernet(manager, mac, IPAddress(192,168,1,200));

void setup() {

  manager.beginSerial();
  manager.initialize();
}

void loop() {
  manager.pause(5000);
}