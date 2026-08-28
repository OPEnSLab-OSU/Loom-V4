#pragma once

#include <Arduino.h>

/**
 * Minimal ADS1232 driver with bounded ready waits and correct 24-bit retrieval.
 * Output is converted from two's-complement to offset-binary, centered at 2^23.
 */
class ADS1232_Lib_Fixed {
  public:
    byte PDWN;
    byte SCLK;
    byte DOUT;
    long OFFSET = 0;
    float SCALE = 1.0f;

    ADS1232_Lib_Fixed(byte pdwn, byte sclk, byte dout);

    bool is_ready() const;
    bool power_up(uint32_t timeoutMs = 1500);
    void power_down();
    void set_offset(long offset = 0);
    void set_scale(float scale = 1.0f);
    long raw_read(byte times = 1, uint32_t timeoutMs = 1500);
    float units_read(byte times = 1, uint32_t timeoutMs = 1500);
    bool last_read_valid() const { return lastReadValid; }
    bool is_powered() const { return powered; }

  private:
    bool waitUntilReady(uint32_t timeoutMs) const;
    bool readOne(long &value, uint32_t timeoutMs);
    bool lastReadValid = false;
    bool powered = false;
};
