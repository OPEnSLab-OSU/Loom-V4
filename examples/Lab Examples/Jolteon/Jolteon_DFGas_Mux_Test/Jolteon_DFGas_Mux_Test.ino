/**
 * Three DFRobot Multi-Gas sensors behind a TCA9548A mux.
 *
 * Wiring:
 * DFGAS 1 on mux channel 0
 * DFGAS 2 on mux channel 1
 * DFGAS 3 on mux channel 2
 *
 * DFRobot Multi-Gas sensor address is normally 0x74 or 0x75.
 *
 * Board rail control:
 * GPIO5 LOW  enables the switched 3.3V rail and root I2C pullups.
 * GPIO6 HIGH enables the switched 5V rail, TCA9548A, and gas sensors.
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>
#include <Hardware/Loom_Multiplexer/Loom_Multiplexer.h>

#include <vector>
#include <Wire.h>

static constexpr uint8_t THREE_V_RAIL_ENABLE_PIN = 5;
static constexpr uint8_t FIVE_V_RAIL_ENABLE_PIN = 6;

static constexpr uint8_t TCA9548A_ADDRESS = 0x71;

/*
 * Select the connector currently being tested:
 *
 * 0 = DFGAS 1
 * 1 = DFGAS 2
 * 2 = DFGAS 3
 */
static constexpr uint8_t TEST_MUX_PORT = 1;

static constexpr uint32_t POWER_RAIL_STARTUP_DELAY_MS = 2000;
static constexpr uint32_t MEASUREMENT_INTERVAL_MS = 5000;

static constexpr bool RUN_RAW_I2C_SCAN = true;
static constexpr bool RUN_MUX_PREFLIGHT = true;
static constexpr bool MUX_DEBUG_ENABLED = true;
static constexpr bool VERBOSE_MUX_SCAN = false;

Manager manager("Device", 1);

std::vector<byte> muxSensorAddresses = {
    0x74,
    0x75
};

Loom_Multiplexer mux(manager, muxSensorAddresses);

static void enableSensorPowerRails() {
    /*
     * Set the output latches before changing the pins to OUTPUT.
     *
     * GPIO5 LOW:  3.3V rail enabled
     * GPIO6 HIGH: 5V rail enabled
     */
    digitalWrite(THREE_V_RAIL_ENABLE_PIN, LOW);
    digitalWrite(FIVE_V_RAIL_ENABLE_PIN, HIGH);

    pinMode(THREE_V_RAIL_ENABLE_PIN, OUTPUT);
    pinMode(FIVE_V_RAIL_ENABLE_PIN, OUTPUT);

    digitalWrite(THREE_V_RAIL_ENABLE_PIN, LOW);
    digitalWrite(FIVE_V_RAIL_ENABLE_PIN, HIGH);

    delay(POWER_RAIL_STARTUP_DELAY_MS);
}

static bool probeI2CAddress(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

static bool selectRawMuxPort(uint8_t port) {
    if (port > 7) {
        Serial.println(F("[RAW SCAN] Invalid mux port."));
        return false;
    }

    Wire.beginTransmission(TCA9548A_ADDRESS);
    Wire.write(static_cast<uint8_t>(1u << port));

    const uint8_t result = Wire.endTransmission();

    Serial.print(F("[RAW SCAN] Select mux port "));
    Serial.print(port);
    Serial.print(F(", mask 0x"));

    const uint8_t mask = static_cast<uint8_t>(1u << port);

    if (mask < 0x10) {
        Serial.print('0');
    }

    Serial.print(mask, HEX);
    Serial.print(F(", Wire result "));
    Serial.println(result);

    delay(50);

    return result == 0;
}

static bool disableRawMuxPorts() {
    Wire.beginTransmission(TCA9548A_ADDRESS);
    Wire.write(static_cast<uint8_t>(0x00));

    const uint8_t result = Wire.endTransmission();

    Serial.print(F("[RAW SCAN] Disable all mux ports, Wire result "));
    Serial.println(result);

    return result == 0;
}

static void printHexAddress(uint8_t address) {
    Serial.print(F("0x"));

    if (address < 0x10) {
        Serial.print('0');
    }

    Serial.print(address, HEX);
}

static void rawScanMuxPort(uint8_t port) {
    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("Raw downstream I2C scan"));

    Serial.print(F("[RAW SCAN] Probing TCA9548A at "));
    printHexAddress(TCA9548A_ADDRESS);
    Serial.println();

    if (!probeI2CAddress(TCA9548A_ADDRESS)) {
        Serial.println(F("[RAW SCAN] TCA9548A did not acknowledge."));
        Serial.println(F("========================================"));
        return;
    }

    Serial.println(F("[RAW SCAN] TCA9548A acknowledged."));

    if (!selectRawMuxPort(port)) {
        Serial.println(F("[RAW SCAN] Could not select the requested mux port."));
        Serial.println(F("========================================"));
        return;
    }

    Serial.print(F("[RAW SCAN] Scanning all usable addresses on mux port "));
    Serial.println(port);

    uint8_t foundCount = 0;

    for (uint8_t address = 0x08; address <= 0x77; ++address) {
        /*
         * Skip the mux itself. The upstream TCA remains visible while a
         * downstream channel is selected.
         */
        if (address == TCA9548A_ADDRESS) {
            continue;
        }

        Wire.beginTransmission(address);
        const uint8_t result = Wire.endTransmission();

        if (result == 0) {
            Serial.print(F("[RAW SCAN] Device found at "));
            printHexAddress(address);

            if (address == 0x74 || address == 0x75) {
                Serial.print(F("  <- DFRobot Multi-Gas candidate"));
            }

            Serial.println();
            ++foundCount;
        }

        delay(2);
    }

    if (foundCount == 0) {
        Serial.println(F("[RAW SCAN] No downstream I2C devices acknowledged."));
    } else {
        Serial.print(F("[RAW SCAN] Total downstream devices found: "));
        Serial.println(foundCount);
    }

    disableRawMuxPorts();

    Serial.println(F("Raw downstream I2C scan complete"));
    Serial.println(F("========================================"));
    Serial.println();
}

void setup() {
    manager.beginSerial();

    Serial.println();
    Serial.println(F("Three-channel DFRobot gas sensor test"));
    Serial.println(F("DFGAS 1: mux channel 0"));
    Serial.println(F("DFGAS 2: mux channel 1"));
    Serial.println(F("DFGAS 3: mux channel 2"));

    Serial.print(F("Active test connector: DFGAS "));
    Serial.print(TEST_MUX_PORT + 1);
    Serial.print(F(", mux channel "));
    Serial.println(TEST_MUX_PORT);

    Serial.println(F("Expected gas sensor addresses: 0x74 or 0x75"));

    Serial.println(F("Enabling sensor power rails."));
    Serial.println(F("GPIO5 LOW:  3.3V rail and root I2C pullups enabled."));
    Serial.println(F("GPIO6 HIGH: 5V rail, mux, and gas sensor power enabled."));

    enableSensorPowerRails();

    Serial.print(F("GPIO5 state: "));
    Serial.println(digitalRead(THREE_V_RAIL_ENABLE_PIN));

    Serial.print(F("GPIO6 state: "));
    Serial.println(digitalRead(FIVE_V_RAIL_ENABLE_PIN));

    Serial.println(F("Starting Wire at 100 kHz."));

    Wire.begin();
    Wire.setClock(100000);

    if (RUN_RAW_I2C_SCAN) {
        rawScanMuxPort(TEST_MUX_PORT);
    }

    mux.setDebug(MUX_DEBUG_ENABLED);
    mux.setScanDebug(VERBOSE_MUX_SCAN);

    /*
     * Restrict Loom initialization to the connector under test.
     *
     * DFGAS 1 -> mux channel 0
     * DFGAS 2 -> mux channel 1
     * DFGAS 3 -> mux channel 2
     */
    mux.useOnlyPorts({TEST_MUX_PORT});

    if (RUN_MUX_PREFLIGHT) {
        Serial.println(F("Running non-loading Loom mux preflight scan."));
        mux.debugScan();
        Serial.println(F("Loom mux preflight scan returned."));
    }

    Serial.println(F("Calling manager.initialize()."));

    manager.initialize();

    Serial.println(F("manager.initialize() returned."));
    Serial.println(F("Gas sensor measurement loop starting."));
}

void loop() {
    /*
     * Preserve the required steady-state rail levels.
     */
    digitalWrite(THREE_V_RAIL_ENABLE_PIN, LOW);
    digitalWrite(FIVE_V_RAIL_ENABLE_PIN, HIGH);

    manager.measure();
    manager.package();
    manager.display_data();

    manager.pause(MEASUREMENT_INTERVAL_MS);
}