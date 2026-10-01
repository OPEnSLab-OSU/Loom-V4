#include "Loom_WarningGuards.h"

#include "Loom_Multiplexer.h"
#include "../../Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.h"
#include "Logger.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <Arduino.h>
LOOM_EXTERNAL_INCLUDE_END
#include <cstdarg>

#if LOOM_COMPILE_MUX_DEBUG
#define LOOM_MUX_DEBUG_LOG(...) debugLog(__VA_ARGS__)
#define LOOM_MUX_DEBUG_FORMAT(...) debugLogFormatted(__VA_ARGS__)
#define LOOM_MUX_DEBUG_I2C(...) debugLogI2CResult(__VA_ARGS__)
#else
// Remove diagnostic arguments too: getters and formatting must not run when omitted.
#define LOOM_MUX_DEBUG_LOG(...) ((void)0)
#define LOOM_MUX_DEBUG_FORMAT(...) ((void)0)
#define LOOM_MUX_DEBUG_I2C(...) ((void)0)
#endif

namespace {
void observeMuxSensor(Logger &trace, Module *sensor, byte address, uint8_t port,
                      const void *mux, bool ready, uint32_t objectBytes) {
    const char *name = sensor->getModuleName();
    char gasName[32];
    if (address == 0x74 || address == 0x75) {
        const char *gas = static_cast<Loom_DFMultiGasSensor *>(sensor)->getGasType();
        if (gas != nullptr && gas[0] != '\0') {
            snprintf(gasName, sizeof(gasName), "%.8s / %.19s", gas, name);
            name = gasName; // Actual queried gas type; never infer it from bench port wiring.
        }
    }
    trace.traceObject(name, sensor, objectBytes, mux, port, address, ready);
}

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Multiplexer::Loom_Multiplexer(Manager &man, const LoomMuxSensorLoader *loader)
    : Loom_Multiplexer(man, nullptr, 0, loader) {}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Multiplexer::Loom_Multiplexer(Manager &man, const std::vector<byte> &addresses,
                                 const LoomMuxSensorLoader *loader)
    : Loom_Multiplexer(man, addresses.data(), addresses.size(), loader) {}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Multiplexer::Loom_Multiplexer(Manager &man, std::initializer_list<byte> addresses,
                                 const LoomMuxSensorLoader *loader)
    : Loom_Multiplexer(man, addresses.begin(), addresses.size(), loader) {}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Multiplexer::Loom_Multiplexer(Manager &man, const byte *addresses, size_t count,
                                 const LoomMuxSensorLoader *loader)
    : Module("Multiplexer"), manInst(&man), activeMuxAddr(0), sensorLoader(loader) {
    moduleInitialized = false;
    assignKnownAddresses(addresses, count);
    manInst->registerModule(this);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::assignKnownAddresses(const byte *addresses, size_t count) {
    if (count == 0) {
        addresses = sensorLoader ? sensorLoader->addresses : nullptr;
        count = sensorLoader ? sensorLoader->addressCount : 0;
    }
    if (addresses && count) {
        known_addresses.assign(addresses, addresses + count);
    } else {
        known_addresses.clear();
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Multiplexer::~Loom_Multiplexer() { clearSensors(); }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::setKnownAddresses(const std::vector<byte> &addresses) {
    FUNCTION_START(this);
    assignKnownAddresses(addresses.data(), addresses.size());

    LOOM_MUX_DEBUG_FORMAT("Mux known address count set to %u", (unsigned int)known_addresses.size());
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::enablePort(uint8_t port) {
    FUNCTION_START(this);

    if (port >= numPorts) {
        ERRORF("Mux port %u is out of range", port);
        LOOM_MUX_DEBUG_FORMAT("Mux port %u is out of range", port);
        return;
    }

    portEnabled[port] = true;
    LOOM_MUX_DEBUG_FORMAT("Mux port %u enabled", port);
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::disablePort(uint8_t port) {
    FUNCTION_START(this);

    if (port >= numPorts) {
        ERRORF("Mux port %u is out of range", port);
        LOOM_MUX_DEBUG_FORMAT("Mux port %u is out of range", port);
        return;
    }

    portEnabled[port] = false;
    LOOM_MUX_DEBUG_FORMAT("Mux port %u disabled", port);
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::disablePorts(const std::vector<uint8_t> &ports) {
    FUNCTION_START(this);

    for (uint8_t port : ports) {
        disablePort(port);
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::useOnlyPorts(const std::vector<uint8_t> &ports) {
    FUNCTION_START(this);

    portEnabled.fill(false);

    for (uint8_t port : ports) {
        enablePort(port);
    }

    LOOM_MUX_DEBUG_FORMAT("Mux scan restricted to %u requested port(s)", (unsigned int)ports.size());
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::setTSL2591Options(tsl2591Gain_t light_gain,
                                         tsl2591IntegrationTime_t integration_time) {
    FUNCTION_START(this);
    sensorOptions.tsl2591Gain = light_gain;
    sensorOptions.tsl2591IntegrationTime = integration_time;
    LOOM_MUX_DEBUG_LOG("TSL2591 auto-load options updated");
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::setSEN66Options(bool measurePM, bool readNumVals) {
    FUNCTION_START(this);
    sensorOptions.sen66MeasurePM = measurePM;
    sensorOptions.sen66ReadNumVals = readNumVals;
    LOOM_MUX_DEBUG_LOG("SEN66 auto-load options updated");
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

void Loom_Multiplexer::setDFGasPowerRetained(bool retained) {
    sensorOptions.dfGasPowerRetained = retained;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::setDebug(bool enabled) {
    debugOutput = LOOM_COMPILE_MUX_DEBUG && enabled;

    if (LOOM_COMPILE_MUX_DEBUG && debugOutput) {
        Serial.println(F("[MUX DEBUG] Serial debug enabled"));
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::setScanDebug(bool enabled) {
    scanDebugOutput = LOOM_COMPILE_MUX_DEBUG && enabled;

    if (LOOM_COMPILE_MUX_DEBUG && debugOutput) {
        Serial.print(F("[MUX DEBUG] Scan miss debug "));
        Serial.println(scanDebugOutput ? F("enabled") : F("disabled"));
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::debugScan() {
    FUNCTION_START(this);
    byte previousMuxAddr = activeMuxAddr;
    bool previousInitialized = moduleInitialized;
    byte foundMuxAddr = 0;

    LOOM_MUX_DEBUG_LOG("Starting mux debug scan");
    Wire.begin();

    foundMuxAddr = findMultiplexer();

    if (foundMuxAddr == 0) {
        ERROR(F("Debug scan did not find a TCA9548 mux"));
        LOOM_MUX_DEBUG_LOG("Debug scan did not find a TCA9548 mux");
        return;
    }

    activeMuxAddr = foundMuxAddr;
    moduleInitialized = true;

    LOOM_MUX_DEBUG_FORMAT("Debug scan using mux address 0x%02X", activeMuxAddr);

    if (known_addresses.empty()) {
        assignKnownAddresses(nullptr, 0);
        LOOM_MUX_DEBUG_FORMAT("Known address list was empty, using default list of %u",
                          (unsigned int)known_addresses.size());
    }

    for (int port = 0; port < numPorts; port++) {
        if (!isPortEnabled(port)) {
            LOOM_MUX_DEBUG_FORMAT("Debug scan skipping disabled mux port %i", port);
            continue;
        }

        LOOM_MUX_DEBUG_FORMAT("Debug scan selecting mux port %i", port);
        if (!selectPin(port)) {
            ERRORF("Failed to select mux port %i; skipping it.", port);
            continue;
        }
        delay(50);

        bool foundOnPort = false;

        for (byte addr : known_addresses) {
            if (!shouldScanAddress(addr)) {
                continue;
            }

            // Emit before entering Wire so a lower-core stall still leaves an exact location.
            LOOM_FEED_WATCHDOG();
            LOOM_MUX_DEBUG_FORMAT("Debug scan probing mux port %i at address 0x%02X", port, addr);
            uint8_t result = probeAddress(addr);

            if (result == 0) {
                foundOnPort = true;
                LOOM_MUX_DEBUG_FORMAT("ACK on mux port %i at I2C address 0x%02X", port, addr);
            } else if (scanDebugOutput) {
                LOOM_MUX_DEBUG_FORMAT("No ACK on mux port %i at I2C address 0x%02X, Wire error %u",
                                  port, addr, result);
            }
        }

        if (!foundOnPort) {
            LOOM_MUX_DEBUG_FORMAT("No known devices found on mux port %i", port);
        }
    }

    disableChannels();

    activeMuxAddr = previousMuxAddr;
    moduleInitialized = previousInitialized;

    LOOM_MUX_DEBUG_LOG("Finished mux debug scan");
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::initialize() {
    FUNCTION_START(this);
    Wire.begin();

    LOOM_MUX_DEBUG_LOG("Mux initialize entered");

    // Do not retain an old address or sensor list if a later re-scan fails.
    clearSensors();
    activeMuxAddr = 0;
    moduleInitialized = false;

    if (known_addresses.empty()) {
        assignKnownAddresses(nullptr, 0);
        LOOM_MUX_DEBUG_FORMAT("Known address list was empty, using default list of %u",
                          (unsigned int)known_addresses.size());
    } else {
        LOOM_MUX_DEBUG_FORMAT("Known address list has %u address(es)",
                          (unsigned int)known_addresses.size());
    }

    activeMuxAddr = findMultiplexer();
    if (activeMuxAddr != 0) {
        LOGF("Multiplexer found at address 0x%02X", activeMuxAddr);
        LOOM_MUX_DEBUG_FORMAT("Multiplexer found at address 0x%02X", activeMuxAddr);
        moduleInitialized = true;
        scanAndLoadSensors();
        LOOM_MUX_DEBUG_FORMAT("Mux initialization loaded %u sensor(s)", (unsigned int)sensors.size());
        if (sensors.empty()) {
            ERROR(F("No sensors found!"));
            LOOM_MUX_DEBUG_LOG("No sensors found behind mux");
        }
        disableChannels();
        LOOM_MUX_DEBUG_LOG("Mux initialize finished");
        return;
    }

    ERROR(F("Multiplexer was not found at the standard address or any alternatives"));
    LOOM_MUX_DEBUG_LOG("Multiplexer was not found at 0x70-0x77");
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::refreshSensors() {
    FUNCTION_START(this);

    if (!moduleInitialized) {
        ERROR(F("Cannot refresh sensors because multiplexer is not initialized"));
        LOOM_MUX_DEBUG_LOG("Cannot refresh sensors because mux is not initialized");
        return;
    }

    LOOM_MUX_DEBUG_LOG("Refreshing mux sensors");
    clearSensors();
    scanAndLoadSensors();
    disableChannels();
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::clearSensors() {
    FUNCTION_START(this);
    LOOM_MUX_DEBUG_FORMAT("Clearing %u auto-loaded mux sensor(s)", (unsigned int)sensors.size());

    for (const MuxSensor &sensor : sensors) {
        Logger::getInstance()->retireTraceObject(sensor.module);
        delete sensor.module;
    }

    sensors.clear();
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::scanAndLoadSensors() {
    FUNCTION_START(this);

    if (known_addresses.empty()) {
        assignKnownAddresses(nullptr, 0);
        LOOM_MUX_DEBUG_FORMAT("Known address list was empty, using default list of %u",
                          (unsigned int)known_addresses.size());
    }

    // Most deployments request one address per attached sensor. Reserve that pointer/metadata
    // table before allocating sensor objects so vector growth cannot leave small holes between
    // long-lived objects in the SAMD21 heap. Repeated addresses on several ports can still grow
    // normally and refreshSensors() reuses the resulting capacity.
    if (sensors.capacity() < known_addresses.size()) {
        sensors.reserve(known_addresses.size());
    }

    for (int port = 0; port < numPorts; port++) {
        if (!isPortEnabled(port)) {
            LOGF("Skipping disabled mux port %i", port);
            LOOM_MUX_DEBUG_FORMAT("Skipping disabled mux port %i", port);
            continue;
        }

        LOOM_MUX_DEBUG_FORMAT("Scanning mux port %i", port);

        if (!selectPin(port)) {
            ERRORF("Failed to select mux port %i; skipping it.", port);
            continue;
        }
        delay(50);

        bool foundOnPort = false;

        for (byte addr : known_addresses) {
            if (!shouldScanAddress(addr)) {
                continue;
            }

            // Emit before entering Wire so a lower-core stall still leaves an exact location.
            LOOM_MUX_DEBUG_FORMAT("Probing mux port %i at address 0x%02X", port, addr);
            const uint8_t result = probeAddress(addr);
            if (result != 0) {
                if (scanDebugOutput) {
                    LOOM_MUX_DEBUG_FORMAT("No ACK on mux port %i at I2C address 0x%02X, Wire error %u",
                                      port, addr, result);
                }
                continue;
            }

            // An ACK means hardware is present, even if Loom has no driver for this address.
            foundOnPort = true;
            LOGF("Found I2C device on port %i at address 0x%02X", port, addr);
            LOOM_MUX_DEBUG_FORMAT("Found I2C device on port %i at address 0x%02X", port, addr);

            Module *sensor = loadSensor(addr);
            if (sensor == nullptr) {
                ERRORF("No Loom sensor loader found for I2C address 0x%02X", addr);
                LOOM_MUX_DEBUG_FORMAT("No Loom sensor loader found for I2C address 0x%02X", addr);
                continue;
            }

            char moduleName[MODULE_NAME_SIZE];
            snprintf(moduleName, sizeof(moduleName), "%s_%i", sensor->getModuleName(), port);
            sensor->setModuleName(moduleName);

            if (Logger::getInstance()->hasTrace()) {
                observeMuxSensor(*Logger::getInstance(), sensor, addr, port, this, false, sensorObjectBytes(addr));
            }

            LOOM_MUX_DEBUG_FORMAT("Initializing sensor %s", sensor->getModuleName());
            sensor->initialize();
            LOOM_FEED_WATCHDOG();

            if (!sensor->moduleInitialized) {
                ERRORF("Sensor %s failed initialization and will not be loaded",
                       sensor->getModuleName());
                Logger::getInstance()->retireTraceObject(sensor);
                delete sensor;
                continue;
            }

            // The mux owns successfully loaded sensors and deletes them on refresh/destruction.
            sensors.push_back({addr, sensor, port});
            if (Logger::getInstance()->hasTrace()) {
                observeMuxSensor(*Logger::getInstance(), sensor, addr, port, this, true, sensorObjectBytes(addr));
            }
            LOGF("Loaded sensor %s on port %i", sensor->getModuleName(), port);
            LOOM_MUX_DEBUG_FORMAT("Loaded sensor %s on port %i", sensor->getModuleName(), port);
        }

        if (!foundOnPort) {
            LOOM_MUX_DEBUG_FORMAT("No known devices found on mux port %i", port);
        }

        LOOM_MUX_DEBUG_FORMAT("Finished scanning mux port %i", port);
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::measure() {
    FUNCTION_START(this);

    if (!moduleInitialized) {
        LOOM_MUX_DEBUG_LOG("Mux measure skipped because mux is not initialized");
        return;
    }

    if (sensors.empty()) {
        LOOM_MUX_DEBUG_LOG("Mux measure skipped because no sensors are loaded");
    }

    for (const MuxSensor &sensor : sensors) {
        if (!sensor.module->moduleInitialized) {
            continue;
        }
        FUNCTION_START(sensor.module, "Mux child: measure()");
        LOOM_MUX_DEBUG_FORMAT("Measuring mux sensor %s on port %i", sensor.module->getModuleName(),
                          sensor.port);

        if (!selectPin(sensor.port)) {
            continue;
        }
        delay(50);
        sensor.module->measure();
        LOOM_FEED_WATCHDOG(); // One selected sensor finished; protect the next sensor separately.
    }

    disableChannels();
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

void Loom_Multiplexer::prepareForSampling() {
    FUNCTION_START(this, "Mux: finish startup sensor settling before sample cadence begins");
    if (!moduleInitialized) {
        return;
    }
    for (const MuxSensor &sensor : sensors) {
        if (sensor.module->moduleInitialized && selectPin(sensor.port)) {
            FUNCTION_START(sensor.module, "Mux child: initial sampling preparation");
            LOOM_FEED_WATCHDOG();
            sensor.module->prepareForSampling();
            LOOM_FEED_WATCHDOG();
        }
    }
    disableChannels();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::package() {
    FUNCTION_START(this);

    if (!moduleInitialized) {
        LOOM_MUX_DEBUG_LOG("Mux package skipped because mux is not initialized");
        return;
    }

    if (sensors.empty()) {
        LOOM_MUX_DEBUG_LOG("Mux package skipped because no sensors are loaded");
    }

    for (const MuxSensor &sensor : sensors) {
        if (!sensor.module->moduleInitialized) {
            continue;
        }
        FUNCTION_START(sensor.module, "Mux child: package()");
        LOOM_MUX_DEBUG_FORMAT("Packaging mux sensor %s on port %i", sensor.module->getModuleName(),
                          sensor.port);

        if (!selectPin(sensor.port)) {
            continue;
        }
        sensor.module->package();
    }

    disableChannels();
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::power_up() {
    FUNCTION_START(this);

    if (!moduleInitialized) {
        LOOM_MUX_DEBUG_LOG("Mux power_up is retrying initialization");
        initialize();
        if (!moduleInitialized) {
            return;
        }
    }

    for (const MuxSensor &sensor : sensors) {
        if (!sensor.module->moduleInitialized && !sensor.module->retryPowerUpWhenUninitialized()) {
            continue;
        }
        FUNCTION_START(sensor.module, "Mux child: power_up()");
        LOOM_FEED_WATCHDOG();
        LOOM_MUX_DEBUG_FORMAT("Powering up mux sensor %s on port %i", sensor.module->getModuleName(),
                          sensor.port);

        if (!selectPin(sensor.port)) {
            continue;
        }
        delay(50);
        sensor.module->power_up();
        LOOM_FEED_WATCHDOG();
    }

    disableChannels();
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::power_down() {
    FUNCTION_START(this);

    if (!moduleInitialized) {
        LOOM_MUX_DEBUG_LOG("Mux power_down skipped because mux is not initialized");
        return;
    }

    for (const MuxSensor &sensor : sensors) {
        if (!sensor.module->moduleInitialized && sensor.module->canRemovePower()) {
            continue;
        }
        FUNCTION_START(sensor.module, "Mux child: power_down()");
        LOOM_FEED_WATCHDOG();
        LOOM_MUX_DEBUG_FORMAT("Powering down mux sensor %s on port %i", sensor.module->getModuleName(),
                          sensor.port);

        if (!selectPin(sensor.port)) {
            continue;
        }
        delay(50);
        sensor.module->power_down();
        LOOM_FEED_WATCHDOG();
    }

    disableChannels();
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::idle() {
    FUNCTION_START(this);
    if (!moduleInitialized) {
        return;
    }
    for (const MuxSensor &sensor : sensors) {
        if (sensor.module->moduleInitialized && selectPin(sensor.port)) {
            FUNCTION_START(sensor.module, "Mux child: idle()");
            sensor.module->idle();
        }
        LOOM_FEED_WATCHDOG();
    }
    disableChannels();
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::canRemovePower() const {
    // Manager sees the mux as one module, but its children share the same power rail.
    // Ask every owned child, including one whose initialization or shutdown failed.
    for (const MuxSensor &sensor : sensors) {
        if (!sensor.module->canRemovePower()) {
            return false;
        }
    }
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::resume() {
    FUNCTION_START(this);
    if (!moduleInitialized) {
        return;
    }
    for (const MuxSensor &sensor : sensors) {
        if (sensor.module->moduleInitialized && selectPin(sensor.port)) {
            FUNCTION_START(sensor.module, "Mux child: resume()");
            sensor.module->resume();
        }
        LOOM_FEED_WATCHDOG();
    }
    disableChannels();
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::selectPin(uint8_t pin) {
    FUNCTION_START(this);

    if (pin >= numPorts) {
        LOOM_MUX_DEBUG_FORMAT("Cannot select mux port %u because it is out of range", pin);
        return false;
    }

    if (activeMuxAddr == 0) {
        LOOM_MUX_DEBUG_LOG("Cannot select mux port because no mux address is active");
        return false;
    }

    Wire.beginTransmission(activeMuxAddr);
    Wire.write(1 << pin);
    uint8_t result = Wire.endTransmission();

    LOOM_MUX_DEBUG_FORMAT("Selected mux port %u with mask 0x%02X, Wire result %u", pin,
                      (uint8_t)(1 << pin), result);
    FUNCTION_END;
    return result == 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::disableChannels() {
    FUNCTION_START(this);

    if (activeMuxAddr == 0) {
        LOOM_MUX_DEBUG_LOG("Cannot disable mux channels because no mux address is active");
        return false;
    }

    Wire.beginTransmission(activeMuxAddr);
    Wire.write(0);
    uint8_t result = Wire.endTransmission();

    LOOM_MUX_DEBUG_FORMAT("Disabled all mux channels, Wire result %u", result);
    FUNCTION_END;
    return result == 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::isDeviceConnected(byte addr) {
    FUNCTION_START(this);

    bool response = probeAddress(addr) == 0;
    FUNCTION_END;
    return response;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t Loom_Multiplexer::probeAddress(byte addr) {
    FUNCTION_START(this);
    Wire.beginTransmission(addr);
    return Wire.endTransmission();
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
byte Loom_Multiplexer::findMultiplexer() {
    FUNCTION_START(this);
    for (byte muxAddr : alt_addresses) {
        LOOM_MUX_DEBUG_FORMAT("Checking mux address 0x%02X", muxAddr);
        const uint8_t result = probeAddress(muxAddr);
        LOOM_MUX_DEBUG_I2C("Mux address probe", muxAddr, result);
        if (result == 0 && probeMultiplexer(muxAddr)) {
            return muxAddr;
        }
    }
    return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::probeMultiplexer(byte addr) {
    FUNCTION_START(this);
    // A plain ACK is insufficient because Loom sensors also use 0x70-0x77.
    // A TCA9548 reads back its channel-control mask directly.
    if (Wire.requestFrom((int)addr, 1) != 1) {
        return false;
    }
    const uint8_t originalMask = Wire.read();

    const uint8_t testMasks[] = {0x00, 0x01};
    bool verified = true;
    for (uint8_t mask : testMasks) {
        Wire.beginTransmission(addr);
        Wire.write(mask);
        if (Wire.endTransmission() != 0 || Wire.requestFrom((int)addr, 1) != 1 ||
            Wire.read() != mask) {
            verified = false;
            break;
        }
    }

    // Restore whichever channel mask was active before the probe.
    Wire.beginTransmission(addr);
    Wire.write(originalMask);
    if (Wire.endTransmission() != 0) {
        verified = false;
    }
    return verified;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::isPortEnabled(uint8_t port) {
    FUNCTION_START(this);

    bool enabled = port < numPorts && portEnabled[port];
    FUNCTION_END;
    return enabled;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::shouldScanAddress(byte addr) {
    FUNCTION_START(this);

    bool shouldScan = addr > 0 && addr != activeMuxAddr;
    FUNCTION_END;
    return shouldScan;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
#if LOOM_COMPILE_MUX_DEBUG
void Loom_Multiplexer::debugLog(const char *message) {
    if (debugOutput) {
        Serial.print(F("[MUX DEBUG] "));
        Serial.println(message);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::debugLogFormatted(const char *format, ...) {
    if (!debugOutput) {
        return;
    }

    char output[96];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(output, sizeof(output), format ? format : "", arguments);
    va_end(arguments);
    LOOM_MUX_DEBUG_LOG(output);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::debugLogI2CResult(const char *label, byte addr, uint8_t result) {
    if (!debugOutput) {
        return;
    }

    if (result == 0 || scanDebugOutput) {
        LOOM_MUX_DEBUG_FORMAT("%s 0x%02X: %s, Wire error %u", label, addr, result == 0 ? "ACK" : "NACK",
                          result);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
Module *Loom_Multiplexer::loadSensor(const byte addr) {
    FUNCTION_START(this);

    return sensorLoader && sensorLoader->create ? sensorLoader->create(*manInst, addr, sensorOptions) : nullptr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

void Loom_Multiplexer::traceObjects() {
    if (Logger::getInstance()->hasTrace()) {
        for (const MuxSensor &sensor : sensors) {
            observeMuxSensor(*Logger::getInstance(), sensor.module, sensor.address, sensor.port, this,
                             sensor.module->moduleInitialized, sensorObjectBytes(sensor.address));
        }
    }
}
