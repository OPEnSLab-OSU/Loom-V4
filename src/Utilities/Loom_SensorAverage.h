#pragma once

#include <math.h>
#include <stdint.h>

namespace loomSensor {
// Sensirion reserves UINT16_MAX/INT16_MAX for unavailable fields (not suitable for raw ADC counts).
// One small accumulator per field: an unavailable CO2 sample must not discard valid humidity.
// Intended for short windows (at most 255 samples); no heap, container or history buffer.
class SampleAverage {
  public:
    void addUnsigned(uint16_t raw, float scale = 1.0f) {
        if (raw != UINT16_MAX) {
            add(static_cast<float>(raw), scale);
        }
    }
    void addSigned(int16_t raw, float scale = 1.0f) {
        if (raw != INT16_MAX) {
            add(static_cast<float>(raw), scale);
        }
    }
    float result() const { return samples == 0 ? NAN : sum / samples; }
    bool hasSamples() const { return samples != 0; }

  private:
    float sum = 0.0f;
    uint8_t samples = 0;
    void add(float raw, float scale) {
        if (samples == UINT8_MAX || !isfinite(scale) || scale <= 0.0f) {
            return;
        }
        const float next = sum + raw / scale;
        if (isfinite(next)) {
            sum = next;
            ++samples;
        }
    }
};
} // namespace loomSensor
