/**
 * This is an example use case for Loomified LTE on OPEnS Jolteon SARA-R5 hardware.
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>
#include "arduino_secrets.h"
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>

static constexpr uint8_t LTE_SUPPLY_RAIL_ENABLE_PIN = 6;
static constexpr uint32_t LTE_SUPPLY_STARTUP_DELAY_MS = 1000;

Manager manager("Device", 1);

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

    Serial.println(F("Enabling Jolteon LTE supply rail on GPIO6."));
    enableLTESupplyRail();

    manager.initialize();
}

void loop() {
    lte.verifyConnection();
    manager.pause(5000);
}