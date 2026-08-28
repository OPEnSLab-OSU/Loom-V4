#include "ADS1232_Lib_Fixed.h"

ADS1232_Lib_Fixed::ADS1232_Lib_Fixed(byte pdwn, byte sclk, byte dout)
    : PDWN(pdwn), SCLK(sclk), DOUT(dout) {
    pinMode(PDWN, OUTPUT);
    pinMode(SCLK, OUTPUT);
    pinMode(DOUT, INPUT);
    digitalWrite(SCLK, LOW);
    digitalWrite(PDWN, LOW);
}

bool ADS1232_Lib_Fixed::is_ready() const { return digitalRead(DOUT) == LOW; }

bool ADS1232_Lib_Fixed::waitUntilReady(uint32_t timeoutMs) const {
    const uint32_t start = millis();
    while (!is_ready()) {
        if ((uint32_t)(millis() - start) >= timeoutMs)
            return false;
        delay(1);
    }
    return true;
}

bool ADS1232_Lib_Fixed::power_up(uint32_t timeoutMs) {
    digitalWrite(SCLK, LOW);
    digitalWrite(PDWN, HIGH);
    powered = waitUntilReady(timeoutMs);
    return powered;
}

void ADS1232_Lib_Fixed::power_down() {
    digitalWrite(SCLK, LOW);
    digitalWrite(PDWN, LOW);
    lastReadValid = false;
    powered = false;
}

void ADS1232_Lib_Fixed::set_offset(long offset) { OFFSET = offset; }

void ADS1232_Lib_Fixed::set_scale(float scale) { SCALE = scale; }

bool ADS1232_Lib_Fixed::readOne(long &value, uint32_t timeoutMs) {
    if (!powered || !waitUntilReady(timeoutMs))
        return false;

    uint32_t raw = 0;
    raw |= (uint32_t)shiftIn(DOUT, SCLK, MSBFIRST) << 16;
    raw |= (uint32_t)shiftIn(DOUT, SCLK, MSBFIRST) << 8;
    raw |= (uint32_t)shiftIn(DOUT, SCLK, MSBFIRST);

    // The 25th clock forces DRDY/DOUT high so the next low transition denotes
    // a genuinely new conversion instead of the result just consumed.
    digitalWrite(SCLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(SCLK, LOW);

    value = (long)(raw ^ 0x800000UL) - OFFSET;
    return true;
}

long ADS1232_Lib_Fixed::raw_read(byte times, uint32_t timeoutMs) {
    const byte sampleCount = times == 0 ? 1 : times;
    int64_t sum = 0;
    byte validSamples = 0;

    for (byte i = 0; i < sampleCount; ++i) {
        long value = 0;
        if (!readOne(value, timeoutMs))
            break;
        sum += value;
        ++validSamples;
    }

    lastReadValid = validSamples == sampleCount;
    return validSamples > 0 ? (long)(sum / validSamples) : 0;
}

float ADS1232_Lib_Fixed::units_read(byte times, uint32_t timeoutMs) {
    const long raw = raw_read(times, timeoutMs);
    if (!lastReadValid || SCALE == 0.0f)
        return 0.0f;
    return raw / SCALE;
}
