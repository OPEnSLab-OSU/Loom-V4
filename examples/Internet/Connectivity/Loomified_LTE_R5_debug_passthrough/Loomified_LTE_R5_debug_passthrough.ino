/**
 * Loom LTE SARA-R5 debug using the old OPEnS power path plus extra logs.
 * Serial Monitor: 115200 baud, Both NL & CR.
 */

#define NETWORK_NAME "hologram"
#define NETWORK_USER ""
#define NETWORK_PASS ""

#include <Loom_Manager.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>

Manager manager("Device", 1);

// Explicit OPEnS mode is important. The default constructor path is SparkFun style.
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
