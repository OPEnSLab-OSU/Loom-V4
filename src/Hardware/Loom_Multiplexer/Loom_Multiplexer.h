#pragma once

#include "Loom_WarningGuards.h"

#include "../../Loom_Manager.h"
#include "../../Module.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include "Wire.h"
LOOM_EXTERNAL_INCLUDE_END
#include <array>
#include <initializer_list>
#include <vector>
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <Adafruit_TSL2591.h>
LOOM_EXTERNAL_INCLUDE_END

/**
 * Adds hot swappable functionality for TCA9548 I2C multiplexers.
 *
 * By default the multiplexer scans enabled ports and loads matching Loom I2C
 * sensors automatically. Sensor-specific options can be configured before
 * manager.initialize() without manually attaching sensors to mux ports.
 *
 * NOTE: This significantly increases flash size and resulting storage used.
 *
 * @author Will Richards
 */
class Loom_Multiplexer : public Module {
  public:
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Main cycle: select each sensor's port before using its driver.
    ////////////////////////////////////////////////////////////////////////////////////////////////
    void initialize() override;
    void prepareForSampling() override;
    void measure() override;
    void package() override;
    void power_down() override;
    void power_up() override;
    void idle() override;
    void resume() override;
    bool canRemovePower() const override;
    bool retryPowerUpWhenUninitialized() const override { return true; }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Discovery settings: choose addresses, ports, and sensor options before initialize().
    ////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * Construct a new Multiplexer using the default Loom I2C address list.
     *
     * @param man Reference to the manager
     */
    Loom_Multiplexer(Manager &man);

    /**
     * Construct a new Multiplexer with a specified sensor address list.
     *
     * @param man Reference to the manager
     * @param addresses I2C sensor addresses to scan for behind the mux
     */
    Loom_Multiplexer(Manager &man, const std::vector<byte> &addresses);

    /** Construct from a brace list without allocating a temporary std::vector. */
    Loom_Multiplexer(Manager &man, std::initializer_list<byte> addresses);

    // Destructor removes all auto-loaded sensor instances.
    ~Loom_Multiplexer();

    Loom_Multiplexer(const Loom_Multiplexer &) = delete;
    Loom_Multiplexer &operator=(const Loom_Multiplexer &) = delete;

    /**
     * Set the I2C addresses that the mux should scan for.
     */
    void setKnownAddresses(const std::vector<byte> &addresses);

    /**
     * Enable a mux port for auto-scanning.
     */
    void enablePort(uint8_t port);

    /**
     * Disable a mux port for auto-scanning.
     */
    void disablePort(uint8_t port);

    /**
     * Disable multiple mux ports for auto-scanning.
     */
    void disablePorts(const std::vector<uint8_t> &ports);

    /**
     * Restrict auto-scanning to only these mux ports.
     */
    void useOnlyPorts(const std::vector<uint8_t> &ports);

    /**
     * Set the TSL2591 options used when a TSL2591 is auto-loaded.
     */
    void
    setTSL2591Options(tsl2591Gain_t light_gain = TSL2591_GAIN_MED,
                      tsl2591IntegrationTime_t integration_time = TSL2591_INTEGRATIONTIME_100MS);

    /**
     * Set the SEN66 options used when a SEN66 is auto-loaded.
     */
    void setSEN66Options(bool measurePM = true, bool readNumVals = true);
    /** Set before initialize(): true only when gas-board power is retained throughout sleep.
     * Retained boards keep their acquisition settings; unavailable boards still retry setup. */
    void setDFGasPowerRetained(bool retained = true);

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Debug scans: inspect what responds on each port without adding packet fields.
    ////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * Print mux initialization and scan diagnostics directly to Serial.
     */
    void setDebug(bool enabled = true);

    /**
     * Print NACK/no-device lines during debug scans.
     */
    void setScanDebug(bool enabled = true);

    /**
     * Print a non-loading mux scan before manager.initialize().
     */
    void debugScan();

    /** Re-scan enabled ports and rebuild the auto-loaded sensor list. */
    void refreshSensors();

    /** Debug-only inventory of existing mux children. No scan or allocation is performed. */
    void traceObjects();

  private:
    Manager *manInst;           // Instance of the manager
    byte activeMuxAddr;         // Active TCA9548 address
    const uint8_t numPorts = 8; // Number of ports on the multiplexer

    struct MuxSensor {
        byte address;
        Module *module; // Owned by the mux; clearSensors() deletes it.
        int port;
    };
    std::vector<MuxSensor> sensors;

    Loom_Multiplexer(Manager &man, const byte *addresses, size_t count);
    void assignKnownAddresses(const byte *addresses, size_t count);
    byte findMultiplexer();

    bool selectPin(uint8_t pin);       // Select which mux port to transmit to
    bool disableChannels();            // Disables all channels on the multiplexer
    bool isDeviceConnected(byte addr); // Check if there is a device at the specified address
    uint8_t probeAddress(byte addr);   // Return raw Wire.endTransmission status
    bool probeMultiplexer(byte addr);  // Verify TCA9548 control-register behavior
    bool isPortEnabled(uint8_t port);  // Check if a mux port should be scanned
    bool shouldScanAddress(byte addr); // Check if an address should be scanned behind the mux

    void clearSensors();                 // Deletes auto-loaded sensor instances
    void scanAndLoadSensors();           // Scans enabled ports and loads matching sensors
    Module *loadSensor(const byte addr); // Load the correct sensor based on the I2C address

    void debugLog(const char *message); // Print a diagnostic line when debug is enabled
    void debugLogFormatted(const char *format, ...);
    void debugLogI2CResult(const char *label, byte addr, uint8_t result);

    std::vector<byte> known_addresses = {};
    std::array<bool, 8> portEnabled = {true, true, true, true, true, true, true, true};

    tsl2591Gain_t tsl2591Gain = TSL2591_GAIN_MED;
    tsl2591IntegrationTime_t tsl2591IntegrationTime = TSL2591_INTEGRATIONTIME_100MS;

    bool sen66MeasurePM = true;
    bool sen66ReadNumVals = true;
    bool dfGasPowerRetained = false; // Preserve the existing power-cycle behavior by default.

    bool debugOutput = false;
    bool scanDebugOutput = false;

    /**
     * Possible TCA9548 addresses.
     */
    const std::array<byte, 8> alt_addresses = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77};
};
