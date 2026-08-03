/**
 * This is an example use case for Loomified LTE
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>

// Loom Modules
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>

Manager manager("Device", 1);

// Hypnos supplies the 3.3 V and 5 V rails used by the LTE hardware. SD logging
// is disabled for this connectivity-only example.
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true, false);

// This example targets the SparkFun SARA-R4 carrier: A5 power control, 9600-baud UART.
Loom_LTE lte(manager, NETWORK_NAME, NETWORK_USER, NETWORK_PASS,
             A5, SPARKFUN, -1, LTE_MODEM::SARA_R4);

void setup() {
  manager.beginSerial();

  // Pass the sketch timestamp before Hypnos initializes the RTC, then enable
  // the peripheral rails before Manager initializes the LTE module.
  hypnos.setCompileTime(__DATE__, __TIME__);
  hypnos.enable();
  manager.initialize();
}

void loop() {
  lte.verifyConnection();
  manager.pause(5000);
}
