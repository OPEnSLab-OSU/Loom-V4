#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <array>
// A register-level ADS1115 fake. It models transfer faults and conversion status,
// not electrical I2C timing, watchdog reset or Arduino Wire's internal blocking.
struct FakeWire {
    std::array<uint16_t, 4> registers = {{0, 0x8583, 0, 0}};
    std::vector<uint8_t> transmit;
    std::vector<int16_t> samples;
    size_t sample = 0;
    size_t writeCalls = 0, endCalls = 0, requestCalls = 0;
    size_t failWrite = SIZE_MAX, failEnd = SIZE_MAX, failRequest = SIZE_MAX;
    size_t conversions = 0;
    uint8_t pointer = 0, readByte = 0;
    uint16_t reply = 0;
    bool active = false, neverReady = false, mismatch = false, connected = true;
    uint8_t busyReads = 0, busyReadsRemaining = 0;
    void begin() {}
    void beginTransmission(uint8_t) { transmit.clear(); }
    size_t write(uint8_t value) {
        if (writeCalls++ == failWrite) {
            return 0;
        }
        transmit.push_back(value);
        return 1;
    }
    uint8_t endTransmission(bool = true) {
        if (endCalls++ == failEnd || !connected) {
            return 4;
        }
        if (!transmit.empty()) {
            pointer = transmit[0];
        }
        if (transmit.size() == 3) {
            registers[pointer] = uint16_t((uint16_t(transmit[1]) << 8) | transmit[2]);
            if (pointer == 1) {
                active = true;
                busyReadsRemaining = busyReads;
                ++conversions;
            }
        }
        return 0;
    }
    uint8_t requestFrom(uint8_t, uint8_t length) {
        if (requestCalls++ == failRequest) {
            return 1;
        }
        reply = registers[pointer];
        if (pointer == 1 && active) {
            if (neverReady || busyReadsRemaining) {
                reply &= 0x7FFF;
                if (busyReadsRemaining) {
                    --busyReadsRemaining;
                }
            }
            if (mismatch) {
                reply ^= 0x0200;
            }
        }
        if (pointer == 0) {
            reply = sample < samples.size() ? static_cast<uint16_t>(samples[sample++]) : 0;
            active = false;
        }
        readByte = 0;
        return length;
    }
    int read() { return readByte++ == 0 ? reply >> 8 : reply & 0xFF; }
};
extern FakeWire Wire;
