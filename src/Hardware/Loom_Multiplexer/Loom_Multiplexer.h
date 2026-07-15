#pragma once
#include "../../Loom_Manager.h"
#include "../../Module.h"

#include <array>
#include <vector>
#include <tuple>
#include <algorithm>
#include "Wire.h"
// I2C Sensors Used by Loom
#include "../../Sensors/I2C/Loom_ADS1115/Loom_ADS1115.h"
#include "../../Sensors/I2C/Loom_MPU6050/Loom_MPU6050.h"
#include "../../Sensors/I2C/Loom_MS5803/Loom_MS5803.h"
#include "../../Sensors/I2C/Loom_SHT31/Loom_SHT31.h"
#include "../../Sensors/I2C/Loom_TSL2591/Loom_TSL2591.h"
#include "../../Sensors/I2C/Loom_STEMMA/Loom_STEMMA.h"
#include "../../Sensors/I2C/Loom_MB1232/Loom_MB1232.h"
#include "../../Sensors/I2C/Loom_K30/Loom_K30.h"
#include "../../Sensors/I2C/Loom_MMA8451/Loom_MMA8451.h"
#include "../../Sensors/I2C/Loom_ZXGesture/Loom_ZXGesture.h"
#include "../../Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.h"
#include "../../Sensors/I2C/Loom_T6793/Loom_T6793.h"
#include "../../Sensors/I2C/Loom_SEN55/Loom_SEN55.h"
#include "../../Sensors/I2C/Loom_SEN66/Loom_SEN66.h"

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
class Loom_Multiplexer : public Module{
    public:

        /* Loomified generalized calls */
        void initialize() override;
        void measure() override;
        void package() override;
        void power_down() override;
        void power_up() override;

        /**
         * Construct a new Multiplexer using the default Loom I2C address list.
         *
         * @param man Reference to the manager
         */
        Loom_Multiplexer(Manager& man);

        /**
         * Construct a new Multiplexer with a specified sensor address list.
         *
         * @param man Reference to the manager
         * @param addresses I2C sensor addresses to scan for behind the mux
         */
        Loom_Multiplexer(Manager& man, const std::vector<byte>& addresses);

        // Destructor removes all auto-loaded sensor instances.
        ~Loom_Multiplexer();

        /**
         * Set the I2C addresses that the mux should scan for.
         */
        void setKnownAddresses(const std::vector<byte>& addresses);

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
        void disablePorts(const std::vector<uint8_t>& ports);

        /**
         * Restrict auto-scanning to only these mux ports.
         */
        void useOnlyPorts(const std::vector<uint8_t>& ports);

        /**
         * Set the TSL2591 options used when a TSL2591 is auto-loaded.
         */
        void setTSL2591Options(
            tsl2591Gain_t light_gain = TSL2591_GAIN_MED,
            tsl2591IntegrationTime_t integration_time = TSL2591_INTEGRATIONTIME_100MS
        );

        /**
         * Set the SEN66 options used when a SEN66 is auto-loaded.
         */
        void setSEN66Options(
            bool measurePM = true,
            bool readNumVals = true
        );

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

    private:
        Manager* manInst;                                       // Instance of the manager
        byte activeMuxAddr;                                     // Active TCA9548 address
        const uint8_t numPorts = 8;                             // Number of ports on the multiplexer

        std::vector<std::tuple<byte, Module*, int>> sensors;    // List of auto-loaded sensors

        bool selectPin(uint8_t pin);                            // Select which mux port to transmit to
        bool disableChannels();                                 // Disables all channels on the multiplexer
        bool isDeviceConnected(byte addr);                      // Check if there is a device at the specified address
        uint8_t probeAddress(byte addr);                        // Return raw Wire.endTransmission status
        bool probeMultiplexer(byte addr);                       // Verify TCA9548 control-register behavior
        bool isPortEnabled(uint8_t port);                       // Check if a mux port should be scanned
        bool shouldScanAddress(byte addr);                      // Check if an address should be scanned behind the mux

        void clearSensors();                                    // Deletes auto-loaded sensor instances
        void scanAndLoadSensors();                              // Scans enabled ports and loads matching sensors
        Module* loadSensor(const byte addr);                    // Load the correct sensor based on the I2C address

        void debugLog(const char* message);                     // Print a diagnostic line when debug is enabled
        void debugLogI2CResult(const char* label, byte addr, uint8_t result);

        std::vector<byte> known_addresses = {};
        std::array<bool, 8> portEnabled = {
            true,
            true,
            true,
            true,
            true,
            true,
            true,
            true
        };

        tsl2591Gain_t tsl2591Gain = TSL2591_GAIN_MED;
        tsl2591IntegrationTime_t tsl2591IntegrationTime = TSL2591_INTEGRATIONTIME_100MS;

        bool sen66MeasurePM = true;
        bool sen66ReadNumVals = true;

        bool debugOutput = false;
        bool scanDebugOutput = false;

        // Used to optimize searching for sensors:
        // search addresses in array rather than 0-127.
        const std::vector<byte> default_addresses =
        {
            0x10, ///< ZXGESTURESENSOR
            0x11, ///< ZXGESTURESENSOR
            0x15, ///< T6793
            0x1C, ///< MMA8451
            0x1D, ///< MMA8451
            0x29, ///< TSL2591
            0x36, ///< STEMMA
            0x44, ///< SHT31D
            0x45, ///< SHT31D
            0x48, ///< ADS1115
            0x69, ///< MPU6050 / SEN55
            0x6B, ///< SEN66
            0x70, ///< MB1232
            0x74, ///< DFMultiGasSensor
            0x75, ///< DFMultiGasSensor
            0x76, ///< MS5803
            0x77  ///< MS5803
        };

        /**
         * Possible TCA9548 addresses.
         */
        const std::array<byte, 8> alt_addresses = {
            0x70,
            0x71,
            0x72,
            0x73,
            0x74,
            0x75,
            0x76,
            0x77
        };
};
