/**
 * This is an example use case for Loomified LTE
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>


Manager manager("Device", 1);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIMEZONE::PST);

Loom_LTE lte(manager, NETWORK_NAME, NETWORK_USER, NETWORK_PASS);


void setup() 
{
  manager.beginSerial();

  hypnos.enable();

  manager.initialize();
}

void loop()
{
  lte.verifyConnection();

  manager.pause(5000);
}