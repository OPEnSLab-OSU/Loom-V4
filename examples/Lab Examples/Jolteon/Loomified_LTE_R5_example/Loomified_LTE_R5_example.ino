/**
 * This is an example use case for Loomified LTE on OPEnS Jolteon SARA-R5 hardware.
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>

// Loom Modules
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>

// Select SARA-R5 in Loom_LTE_Config.h or with the project-wide compiler flag
// -DLOOM_LTE_USE_SARA_R5. A define placed only in this sketch cannot configure
// the separately compiled Loom_LTE.cpp translation unit.
#if !defined(LOOM_LTE_USE_SARA_R5)
#error "This example requires the library-wide SARA-R5 LTE profile"
#endif

Manager manager("Device", 1);

Loom_LTE lte(manager, NETWORK_NAME, NETWORK_USER, NETWORK_PASS, A5, OPENS);

void setup() {
  manager.beginSerial();
  manager.initialize();
}

void loop() {
  lte.verifyConnection();
  manager.pause(5000);
}
