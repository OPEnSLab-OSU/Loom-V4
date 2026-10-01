#pragma once

#include "Loom_MuxSensorLoader.h"
#include "../../Sensors/I2C/Loom_ADS1115/Loom_ADS1115.h"
#include "../../Sensors/I2C/Loom_AS7262/Loom_AS7262.h"
#include "../../Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.h"
#include "../../Sensors/I2C/Loom_MB1232/Loom_MB1232.h"
#include "../../Sensors/I2C/Loom_MMA8451/Loom_MMA8451.h"
#include "../../Sensors/I2C/Loom_MS5803/Loom_MS5803.h"
#include "../../Sensors/I2C/Loom_SEN55/Loom_SEN55.h"
#include "../../Sensors/I2C/Loom_SEN66/Loom_SEN66.h"
#include "../../Sensors/I2C/Loom_SHT31/Loom_SHT31.h"
#include "../../Sensors/I2C/Loom_STEMMA/Loom_STEMMA.h"
#include "../../Sensors/I2C/Loom_T6793/Loom_T6793.h"
#include "../../Sensors/I2C/Loom_TSL2591/Loom_TSL2591.h"
#include "../../Sensors/I2C/Loom_ZXGesture/Loom_ZXGesture.h"
#include <array>

namespace LoomMuxDetail {
template <byte Address> struct SensorFactory {
    static_assert(Address != Address, "Unsupported LOOM_MUX_COMPILED_ADDRESSES address; use a supported Loom sensor address");
};

// These bodies generate firmware only when their address is selected. Merely including
// a driver header does not retain its constructors or measurement implementation.
#define LOOM_MUX_FACTORY(address, sensorType, ...)                                              \
    template <> struct SensorFactory<address> {                                               \
        static Module *create(Manager &manager, const LoomMuxSensorOptions &options) {          \
            (void)options;                                                                    \
            return new sensorType(__VA_ARGS__);                                                \
        }                                                                                     \
        static uint32_t objectBytes() { return sizeof(sensorType); }                           \
    };

LOOM_MUX_FACTORY(0x29, Loom_TSL2591, manager, 0x29, true, options.tsl2591Gain, options.tsl2591IntegrationTime)
LOOM_MUX_FACTORY(0x10, Loom_ZXGesture, manager, 0x10, true)
LOOM_MUX_FACTORY(0x11, Loom_ZXGesture, manager, 0x11, true)
LOOM_MUX_FACTORY(0x44, Loom_SHT31, manager, 0x44, true)
LOOM_MUX_FACTORY(0x45, Loom_SHT31, manager, 0x45, true)
LOOM_MUX_FACTORY(0x48, Loom_ADS1115, manager, 0x48, true)
LOOM_MUX_FACTORY(0x49, Loom_AS7262, manager, true, 0x49)
LOOM_MUX_FACTORY(0x1C, Loom_MMA8451, manager, 0x1C, true)
LOOM_MUX_FACTORY(0x1D, Loom_MMA8451, manager, 0x1D, true)
LOOM_MUX_FACTORY(0x74, Loom_DFMultiGasSensor, manager, 0x74, 10, !options.dfGasPowerRetained, true)
LOOM_MUX_FACTORY(0x75, Loom_DFMultiGasSensor, manager, 0x75, 10, !options.dfGasPowerRetained, true)
LOOM_MUX_FACTORY(0x15, Loom_T6793, manager, 0x15, 10, true)
LOOM_MUX_FACTORY(0x69, Loom_SEN55, manager, true, true, true)
LOOM_MUX_FACTORY(0x6B, Loom_SEN66, manager, options.sen66MeasurePM, true, options.sen66ReadNumVals)
LOOM_MUX_FACTORY(0x76, Loom_MS5803, manager, 0x76, true)
LOOM_MUX_FACTORY(0x77, Loom_MS5803, manager, 0x77, true)
LOOM_MUX_FACTORY(0x36, Loom_STEMMA, manager, 0x36, true)
LOOM_MUX_FACTORY(0x70, Loom_MB1232, manager, 0x70, true)
#undef LOOM_MUX_FACTORY

template <byte... Addresses> struct SensorSelection;

template <> struct SensorSelection<> {
    static Module *create(Manager &, byte, const LoomMuxSensorOptions &) { return nullptr; }
    static uint32_t objectBytes(byte) { return 0; }
};

template <byte First, byte... Rest> struct SensorSelection<First, Rest...> {
    static Module *create(Manager &manager, byte address, const LoomMuxSensorOptions &options) {
        return address == First ? SensorFactory<First>::create(manager, options)
                                : SensorSelection<Rest...>::create(manager, address, options);
    }
    static uint32_t objectBytes(byte address) {
        return address == First ? SensorFactory<First>::objectBytes()
                                : SensorSelection<Rest...>::objectBytes(address);
    }
};
} // namespace LoomMuxDetail

template <byte... Addresses> const LoomMuxSensorLoader *loomMuxSelectedSensors() {
    static_assert(sizeof...(Addresses) > 0, "LOOM_MUX_COMPILED_ADDRESSES must contain at least one sensor address");
    static const std::array<byte, sizeof...(Addresses)> addresses = {{Addresses...}};
    static const LoomMuxSensorLoader loader = {
        &LoomMuxDetail::SensorSelection<Addresses...>::create,
        &LoomMuxDetail::SensorSelection<Addresses...>::objectBytes,
        addresses.data(), addresses.size()};
    return &loader;
}
