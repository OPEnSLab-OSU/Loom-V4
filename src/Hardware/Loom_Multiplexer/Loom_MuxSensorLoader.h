#pragma once

#include "../../Module.h"
#include "Loom_WarningGuards.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <Adafruit_TSL2591.h>
LOOM_EXTERNAL_INCLUDE_END

class Manager;

// The same layout is used in every translation unit, regardless of sketch selection.
struct LoomMuxSensorOptions {
    tsl2591Gain_t tsl2591Gain = TSL2591_GAIN_MED;
    tsl2591IntegrationTime_t tsl2591IntegrationTime = TSL2591_INTEGRATIONTIME_100MS;
    bool sen66MeasurePM = true;
    bool sen66ReadNumVals = true;
    bool dfGasPowerRetained = false;
};

struct LoomMuxSensorLoader {
    Module *(*create)(Manager &, byte, const LoomMuxSensorOptions &);
    uint32_t (*objectBytes)(byte);
    const byte *addresses;
    size_t addressCount;
};

// Only callers using the legacy constructor default reference the full loader.
extern const LoomMuxSensorLoader loomMuxAllSensors;
