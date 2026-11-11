// Use the production driver with a GPIO model of the AS5311 SSI interface.
#include <assert.h>
#include <math.h>
#include <vector>
#include "../../src/Sensors/SPI/Loom_AS5311/AS5311.cpp"

namespace {
constexpr uint8_t Cs = 1, Clock = 2, Data = 3;
std::vector<uint32_t> words;
size_t wordIndex = 0;
size_t magnitudeIndex = 0;
uint8_t shifted = 0, sampled = 0;
bool latched = false;
uint8_t modes[4] = {}, levels[4] = {LOW, HIGH, HIGH, LOW};
unsigned long elapsedUs = 0, selectedAt = 0, edgeAt = 0;
// Count parity independently of the production reader; works with MSVC and GCC.
uint32_t parity(uint32_t value) {
    uint32_t result = 0;
    while (value) {
        result ^= value & 1U;
        value >>= 1;
    }
    return result;
}
uint32_t conversion(uint16_t position, uint32_t flags = 0) {
    uint32_t data = (static_cast<uint32_t>(position) << DATAOFFSET) | (1UL << OCF) | flags;
    return data | static_cast<uint32_t>(parity(data)); // Even parity includes bit 0.
}
void supply(const std::vector<uint32_t> &input, bool lastIsMagnitude = false) {
    words = input;
    wordIndex = 0;
    magnitudeIndex = words.size() - (lastIsMagnitude ? 1 : 0);
}
} // namespace
void pinMode(uint8_t pin, uint8_t mode) { modes[pin] = mode; }

void digitalWrite(uint8_t pin, uint8_t value) {
    if (modes[pin] != OUTPUT || value == levels[pin]) {
        return;
    }
    levels[pin] = value;
    if (pin == Cs) {
        if (value == LOW) {
            assert(wordIndex < words.size());
            assert(levels[Clock] == (wordIndex == magnitudeIndex ? LOW : HIGH));
            shifted = sampled = 0;
            latched = false;
            selectedAt = elapsedUs;
        } else {
            assert(sampled == 18 && shifted == 18);
            ++wordIndex;
        }
    } else if (pin == Clock && levels[Cs] == LOW) {
        if (value == LOW && !latched) {
            assert(elapsedUs - selectedAt >= 1);
            latched = true;
        } else if (value == HIGH && latched) {
            assert(shifted == sampled);
            ++shifted;
            edgeAt = elapsedUs;
        }
    }
}

void delayMicroseconds(unsigned int delayUs) { elapsedUs += delayUs; }

int digitalRead(uint8_t pin) {
    assert(pin == Data && modes[pin] == INPUT);
    assert(levels[Cs] == LOW && levels[Clock] == HIGH && latched);
    assert(elapsedUs - edgeAt >= 1);
    assert(shifted == sampled + 1 && shifted <= 18);
    return (words[wordIndex] >> (17 - sampled++)) & 1;
}

int main() {
    AS5311 sensor(Cs, Clock, Data);
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

    supply({conversion(0xABC), conversion(0x537)}, true);
    assert(sensor.getRawData() == conversion(0xABC));
    assert(sensor.getFieldStrength() == 0x537);

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
    const uint32_t red = conversion(100, 1UL << LIN);
    supply({red}); // Valid parity, invalid linearity.
    assert(sensor.getMagnetStatus() == magnetStatus::red);
    supply({red});
    assert(sensor.getFilteredPosition() == AS5311::INVALID_READING);

    AS5311 packetSensor(Cs, Clock, Data);
    Manager packet;
    supply({red, conversion(11)}, true);
    packetSensor.measure(packet);
    assert(strcmp(packet.alignment, "Red") == 0);
    assert(packet.raw == -1 && packet.average == -1 && packet.magnitude == 11);
    assert(isnan(packet.displacement));

    std::vector<uint32_t> goodSample(16, conversion(116));
    goodSample.front() = good;
    goodSample.push_back(conversion(700));
    supply(goodSample, true);
    packetSensor.measure(packet);
    assert(strcmp(packet.alignment, "Green") == 0);
    assert(packet.raw == 100 && packet.average == 115 && packet.magnitude == 700);
    assert(packet.displacement == 0);

    assert(sensor.measureDisplacement(4095) == 0);
    assert(sensor.measureDisplacement(0) == 2000.0f / 4096);
    assert(isnan(sensor.measureDisplacement(AS5311::INVALID_READING)));
    assert(sensor.measureDisplacement(1) == 2 * 2000.0f / 4096); // Bad read did not move baseline.
    assert(isnan(sensor.measureDisplacement(-1)));
}
