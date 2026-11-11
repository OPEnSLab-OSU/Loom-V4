#pragma once

#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>
#include <Sensors/SPI/Loom_AS5311/AS5311.h>

namespace bench {

struct DendrometerSensors {
    explicit DendrometerSensors(Manager &manager) : climate(manager), encoder(A3, A5, A4) {}

    void setPower(bool enabled) {
        // Hypnos V3.3 rail enables are active-low on D5 and active-high on D6.
        digitalWrite(5, enabled ? LOW : HIGH);
        digitalWrite(6, enabled ? HIGH : LOW);
        pinMode(5, OUTPUT);
        pinMode(6, OUTPUT);
        if (enabled) {
            delay(100);
        }
    }

    Loom_SHT31 climate;
    AS5311 encoder;
};

} // namespace bench
