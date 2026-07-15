/**
 * Loom LTE SARA-R5 debug using the old OPEnS power path plus extra logs.
 * Serial Monitor: 115200 baud, Both NL & CR.
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

// Keep the OPEnS/Jolteon board-power style explicit in this hardware diagnostic.
Loom_LTE lte(manager, NETWORK_NAME, NETWORK_USER, NETWORK_PASS, A5, OPENS);

void setup() {
  manager.beginSerial();

  Serial.println(F("SARA-R5 old-power debug starting."));
  Serial.println(F("Expected hardware: A5=PWR_ON, Serial1=LTE UART, SARA UART=115200."));
  Serial.println(F("This sketch uses the old OPEnS power sequence and extra logs."));

  manager.initialize();

  Serial.println(F("If initialization failed before network, passthrough is still available."));
  Serial.println(F("Type AT, AT+CPIN?, AT+CSQ, AT+CEREG?, AT+CGDCONT?, AT+CGATT?, or AT+CEER."));
}

void loop() {
  lte.debugPassthrough();
}
