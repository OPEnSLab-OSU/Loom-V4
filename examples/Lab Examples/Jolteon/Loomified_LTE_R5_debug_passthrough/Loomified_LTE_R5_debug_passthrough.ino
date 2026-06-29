/**
 * Direct SARA-R5 UART debug bridge.
 * Type AT commands into the Serial Monitor with Both NL & CR enabled.
 */
#define LOOM_LTE_USE_SARA_R5

#include "arduino_secrets.h"
#include <Loom_Manager.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>

Manager manager("Device", 1);

Loom_LTE lte(manager, NETWORK_NAME, NETWORK_USER, NETWORK_PASS);

void setup() {
  manager.beginSerial();
  manager.initialize();

  Serial.println(F("Direct LTE UART bridge ready."));
  Serial.println(F("Useful commands: AT, AT+CPIN?, AT+CSQ, AT+CEREG?, AT+CGDCONT?, AT+CGATT?, AT+CEER"));
}

void loop() {
  lte.debugPassthrough();
}
