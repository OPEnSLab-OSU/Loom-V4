#include "Loom_WarningGuards.h"

#include "Loom_Multiplexer.h"
#include "../../Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.h"
#include "Logger.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <Arduino.h>
LOOM_EXTERNAL_INCLUDE_END

#if LOOM_COMPILE_MUX_DEBUG
#define LOOM_MUX_DEBUG_LINE(...)                                                                  \
    do {                                                                                          \
        if (debugOutput) debugLogFormatted(__VA_ARGS__);                                           \
    } while (false)
#else
#define LOOM_MUX_DEBUG_LINE(...) ((void)0)
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

    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::enablePort(uint8_t port) {
    FUNCTION_START(this);

    if (port >= numPorts) {
        ERRORF("Mux port %u is out of range", port);
        return;
    }

    portEnabled[port] = true;
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::disablePort(uint8_t port) {
    FUNCTION_START(this);

    if (port >= numPorts) {
        ERRORF("Mux port %u is out of range", port);
        return;
    }

    portEnabled[port] = false;
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

    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::setTSL2591Options(tsl2591Gain_t light_gain,
                                         tsl2591IntegrationTime_t integration_time) {
    FUNCTION_START(this);
    sensorOptions.tsl2591Gain = light_gain;
    sensorOptions.tsl2591IntegrationTime = integration_time;
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::setSEN66Options(bool measurePM, bool readNumVals) {
    FUNCTION_START(this);
    sensorOptions.sen66MeasurePM = measurePM;
    sensorOptions.sen66ReadNumVals = readNumVals;
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

void Loom_Multiplexer::setDFGasPowerRetained(bool retained) {
    sensorOptions.dfGasPowerRetained = retained;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::initialize() {
    FUNCTION_START(this);
    LOOM_MUX_DEBUG_LINE("Starting Wire for mux initialization");
    Wire.begin();

    // Do not retain an old address or sensor list if a later re-scan fails.
    clearSensors();
    activeMuxAddr = 0;
    moduleInitialized = false;

    if (known_addresses.empty()) {
        assignKnownAddresses(nullptr, 0);
    }

    activeMuxAddr = findMultiplexer();
    if (activeMuxAddr != 0) {
        LOGF("Multiplexer found at address 0x%02X", activeMuxAddr);
        moduleInitialized = true;
        scanAndLoadSensors();
        if (sensors.empty()) {
            ERROR(F("No sensors found!"));
        }
        disableChannels();
        return;
    }

    ERROR(F("Multiplexer was not found at the standard address or any alternatives"));
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::refreshSensors() {
    FUNCTION_START(this);

    if (!moduleInitialized) {
        ERROR(F("Cannot refresh sensors because multiplexer is not initialized"));
        return;
    }

    clearSensors();
    scanAndLoadSensors();
    disableChannels();
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::clearSensors() {
    FUNCTION_START(this);

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
            continue;
        }

        LOOM_MUX_DEBUG_LINE("Selecting mux port %i for discovery", port);
        if (!selectPin(port)) {
            ERRORF("Failed to select mux port %i; skipping it.", port);
            continue;
        }
        delay(50);

        for (byte addr : known_addresses) {
            if (!shouldScanAddress(addr)) {
                continue;
            }

            // Print before entering Wire so a stalled transaction leaves its address in Serial.
            LOOM_MUX_DEBUG_LINE("Probing mux port %i at address 0x%02X", port, addr);
            const uint8_t result = probeAddress(addr);
            if (result != 0) {
                if (scanDebugOutput) {
                    LOOM_MUX_DEBUG_LINE("No ACK on mux port %i at address 0x%02X, Wire error %u",
                                        port, addr, result);
                }
                continue;
            }

            LOGF("Found I2C device on port %i at address 0x%02X", port, addr);

            Module *sensor = loadSensor(addr);
            if (sensor == nullptr) {
                ERRORF("No Loom sensor loader found for I2C address 0x%02X", addr);
                continue;
            }

            char moduleName[MODULE_NAME_SIZE];
            snprintf(moduleName, sizeof(moduleName), "%s_%i", sensor->getModuleName(), port);
            sensor->setModuleName(moduleName);

            if (Logger::getInstance()->hasTrace()) {
                observeMuxSensor(*Logger::getInstance(), sensor, addr, port, this, false, sensorObjectBytes(addr));
            }
            LOOM_MUX_DEBUG_LINE("Initializing %s on mux port %i at address 0x%02X",
                                sensor->getModuleName(), port, addr);
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
        }

    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::measure() {
    FUNCTION_START(this);

    if (!moduleInitialized) {
        return;
    }

    for (const MuxSensor &sensor : sensors) {
        if (!sensor.module->moduleInitialized) {
            continue;
        }
        FUNCTION_START(sensor.module, "Mux child: measure()");
        LOOM_MUX_DEBUG_LINE("Measuring %s on mux port %u", sensor.module->getModuleName(),
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
        return;
    }

    for (const MuxSensor &sensor : sensors) {
        if (!sensor.module->moduleInitialized) {
            continue;
        }
        FUNCTION_START(sensor.module, "Mux child: package()");
        LOOM_MUX_DEBUG_LINE("Packaging %s on mux port %u", sensor.module->getModuleName(),
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
        LOOM_MUX_DEBUG_LINE("Powering up %s on mux port %u", sensor.module->getModuleName(),
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
        return;
    }

    for (const MuxSensor &sensor : sensors) {
        if (!sensor.module->moduleInitialized && sensor.module->canRemovePower()) {
            continue;
        }
        FUNCTION_START(sensor.module, "Mux child: power_down()");
        LOOM_FEED_WATCHDOG();
        LOOM_MUX_DEBUG_LINE("Powering down %s on mux port %u", sensor.module->getModuleName(),
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
        return false;
    }

    if (activeMuxAddr == 0) {
        return false;
    }

    Wire.beginTransmission(activeMuxAddr);
    Wire.write(1 << pin);
    uint8_t result = Wire.endTransmission();

    FUNCTION_END;
    return result == 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::disableChannels() {
    FUNCTION_START(this);

    if (activeMuxAddr == 0) {
        return false;
    }

    LOOM_MUX_DEBUG_LINE("Disabling mux channels at address 0x%02X", activeMuxAddr);
    Wire.beginTransmission(activeMuxAddr);
    Wire.write(0);
    uint8_t result = Wire.endTransmission();

    FUNCTION_END;
    return result == 0;
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
        LOOM_MUX_DEBUG_LINE("Probing mux controller at address 0x%02X", muxAddr);
        const uint8_t result = probeAddress(muxAddr);
        if (result != 0 && scanDebugOutput) {
            LOOM_MUX_DEBUG_LINE("No ACK from mux controller candidate 0x%02X, Wire error %u",
                                muxAddr, result);
        }
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
    LOOM_MUX_DEBUG_LINE("Reading mux control register at address 0x%02X", addr);
    // A plain ACK is insufficient because Loom sensors also use 0x70-0x77.
    // A TCA9548 reads back its channel-control mask directly.
    if (Wire.requestFrom((int)addr, 1) != 1) {
        return false;
    }
    const uint8_t originalMask = Wire.read();

    const uint8_t testMasks[] = {0x00, 0x01};
    bool verified = true;
    for (uint8_t mask : testMasks) {
        LOOM_MUX_DEBUG_LINE("Verifying mux mask 0x%02X at address 0x%02X", mask, addr);
        Wire.beginTransmission(addr);
        Wire.write(mask);
        if (Wire.endTransmission() != 0 || Wire.requestFrom((int)addr, 1) != 1 ||
            Wire.read() != mask) {
            verified = false;
            break;
        }
    }

    // Restore whichever channel mask was active before the probe.
    LOOM_MUX_DEBUG_LINE("Restoring mux mask 0x%02X at address 0x%02X", originalMask, addr);
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
