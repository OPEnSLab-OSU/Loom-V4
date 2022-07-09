/**
 * This is an example use case for Loomified LTE
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

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
  delay(5000);
}