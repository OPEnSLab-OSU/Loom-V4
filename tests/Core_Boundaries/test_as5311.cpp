// Deferred GPIO/serial-word tests. Build later with as5311_fakes as the include directory;
// including the production example implementation exercises its actual validation/averaging.
#include <assert.h>
#include <math.h>
#include <vector>
#include "../../examples/Lab Examples/Dendrometer/node/AS5311.cpp"

namespace {
std::vector<uint32_t> words;
size_t wordIndex = 0;
uint8_t bitIndex = 0;
// Count parity independently of the production reader; works with MSVC and GCC.
uint32_t parity(uint32_t value) {
    uint32_t result = 0;
    while (value) {
        result ^= value & 1U;
        value >>= 1;
    }
    return result;
}
uint32_t conversion(uint16_t position) {
    uint32_t data = (static_cast<uint32_t>(position) << DATAOFFSET) | (1UL << OCF);
    return data | static_cast<uint32_t>(parity(data)); // Even parity includes bit 0.
}
void supply(const std::vector<uint32_t> &input) {
    words = input;
    wordIndex = 0;
    bitIndex = 0;
}
} // namespace
void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t, uint8_t) {}
void delayMicroseconds(unsigned int) {}
int digitalRead(uint8_t) {
    assert(wordIndex < words.size());
    const int value = (words[wordIndex] >> (17 - bitIndex)) & 1;
    if (++bitIndex == 18) {
        bitIndex = 0;
        ++wordIndex;
    }
    return value;
}
int main() {
    AS5311 sensor(1, 2, 3);
    const uint32_t good = conversion(100);
    assert(AS5311::isValidReading(good));
    assert(!AS5311::isValidReading(0));
    assert(!AS5311::isValidReading(good ^ (1UL << 10))); // One damaged position bit.
    assert(!AS5311::isValidReading(1UL << 18));
    uint32_t overflow = good | (1UL << COF);
    if (parity(overflow)) {
        overflow ^= 1;
    }
    assert(!AS5311::isValidReading(overflow));

    supply(std::vector<uint32_t>(16, conversion(4095)));
    assert(sensor.getFilteredPosition() == 4095);
    for (uint16_t first : {uint16_t(0), uint16_t(4095)}) {
        std::vector<uint32_t> aroundZero;
        for (int i = 0; i < 16; ++i) {
            aroundZero.push_back(conversion(i % 2 ? 4095 - first : first));
        }
        supply(aroundZero);
        const uint16_t average = sensor.getFilteredPosition();
        assert(average == 0 || average == 4095); // Never the false half-range mean, 2047.
    }
    std::vector<uint32_t> corrupted(16, good);
    corrupted[7] ^= 1;
    supply(corrupted);
    assert(sensor.getFilteredPosition() == AS5311::INVALID_READING);
    supply({conversion(100) | (1UL << LIN) | 1}); // Valid parity, invalid linearity.
    assert(sensor.getMagnetStatus() == magnetStatus::red);
    supply({conversion(100) | (1UL << LIN) | 1});
    assert(sensor.getFilteredPosition() == AS5311::INVALID_READING);

    assert(sensor.measureDisplacement(4095) == 0);
    assert(sensor.measureDisplacement(0) == 2000.0f / 4096);
    assert(isnan(sensor.measureDisplacement(AS5311::INVALID_READING)));
    assert(sensor.measureDisplacement(1) == 2 * 2000.0f / 4096); // Bad read did not move baseline.
    assert(isnan(sensor.measureDisplacement(-1)));
}
