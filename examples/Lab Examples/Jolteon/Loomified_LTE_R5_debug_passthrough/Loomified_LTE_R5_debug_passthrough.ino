/**
 * Direct OPEnS Jolteon SARA-R5 UART debug bridge.
 * Type AT commands into the Serial Monitor with Both NL & CR enabled.
 */
#include "arduino_secrets.h"
#include <Loom_Manager.h>
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

  Serial.println(F("Direct LTE UART bridge ready."));
  Serial.println(F("Useful commands: AT, AT+CPIN?, AT+CSQ, AT+CEREG?, AT+CGDCONT?, AT+CGATT?, AT+CEER"));
}

void loop() {
  lte.debugPassthrough();
}
