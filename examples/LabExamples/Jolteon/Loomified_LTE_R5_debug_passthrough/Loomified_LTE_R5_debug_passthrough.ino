/**
 * Loom LTE SARA-R5 debug using the old OPEnS power path plus extra logs.
 * Serial Monitor: 115200 baud, Both NL & CR.
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>
#include "arduino_secrets.h"
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>

static constexpr uint8_t LTE_SUPPLY_RAIL_ENABLE_PIN = 6;
static constexpr uint32_t LTE_SUPPLY_STARTUP_DELAY_MS = 1000;

Manager manager("Device", 1);

// Keep the OPEnS/Jolteon board-power style explicit in this hardware diagnostic.
Loom_LTE lte(
    manager,
    NETWORK_NAME,
    NETWORK_USER,
    NETWORK_PASS,
    A5,
    OPENS,
    -1,
    LTE_MODEM::SARA_R5
);

static void enableLTESupplyRail() {
    digitalWrite(LTE_SUPPLY_RAIL_ENABLE_PIN, HIGH);
    pinMode(LTE_SUPPLY_RAIL_ENABLE_PIN, OUTPUT);
    digitalWrite(LTE_SUPPLY_RAIL_ENABLE_PIN, HIGH);

    delay(LTE_SUPPLY_STARTUP_DELAY_MS);
}

void setup() {
    manager.beginSerial();

    Serial.println(F("SARA-R5 old-power debug starting."));
    Serial.println(F("Enabling Jolteon LTE supply rail on GPIO6."));
    Serial.println(F("Expected hardware: GPIO6=5V rail enable, A5=PWR_ON, Serial1=LTE UART."));
    Serial.println(F("This sketch uses the old OPEnS power sequence and extra logs."));

    enableLTESupplyRail();

    manager.initialize();

    Serial.println(F("If initialization failed before network, passthrough is still available."));
    Serial.println(F("Type AT, AT+CPIN?, AT+CSQ, AT+CEREG?, AT+CGDCONT?, AT+CGATT?, or AT+CEER."));
}

void loop() {
    lte.debugPassthrough();
}