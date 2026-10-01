// Deferred tests: reserved raw sentinels must never enter an average or discard another field.
#include "../../src/Utilities/Loom_SensorAverage.h"
#include <assert.h>

int main() {
    using loomSensor::SampleAverage;
    SampleAverage humidity, co2, temperature;
    humidity.addSigned(5000, 100.0f);
    co2.addUnsigned(UINT16_MAX);
    temperature.addSigned(-1000, 200.0f);
    assert(humidity.result() == 50.0f);
    assert(!co2.hasSamples() && isnan(co2.result()));
    assert(temperature.result() == -5.0f);
    humidity.addSigned(INT16_MAX, 100.0f);
    assert(humidity.result() == 50.0f); // Invalid samples are excluded from the denominator too.
    co2.addUnsigned(400);
    co2.addUnsigned(600);
    co2.addUnsigned(UINT16_MAX);
    assert(co2.result() == 500.0f);

    SampleAverage zero;
    zero.addUnsigned(0, 10.0f);
    assert(zero.hasSamples() && zero.result() == 0.0f);
    SampleAverage boundary;
    boundary.addUnsigned(UINT16_MAX - 1);
    assert(boundary.result() == 65534.0f);
    SampleAverage badScale;
    badScale.addSigned(100, 0);
    badScale.addUnsigned(100, -1);
    badScale.addUnsigned(100, NAN);
    assert(!badScale.hasSamples());
    SampleAverage capped;
    for (unsigned i = 0; i < UINT8_MAX; ++i) {
        capped.addUnsigned(100);
    }
    capped.addUnsigned(10000); // The counter must not wrap to zero on sample 256.
    assert(capped.hasSamples() && capped.result() == 100.0f);
    return 0;
}
