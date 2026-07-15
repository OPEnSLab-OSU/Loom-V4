/**
 * This is an example use case for Loomified LTE on OPEnS Jolteon SARA-R5 hardware.
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>

// Loom Modules
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>

Manager manager("Device", 1);

Loom_LTE lte(manager, NETWORK_NAME, NETWORK_USER, NETWORK_PASS, A5, OPENS, -1,
             LTE_MODEM::SARA_R5);

void setup() {
  manager.beginSerial();
  manager.initialize();
}

void loop() {
  lte.verifyConnection();
  manager.pause(5000);
}
