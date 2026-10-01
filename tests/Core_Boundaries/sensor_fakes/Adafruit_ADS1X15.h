#pragma once
#include <stdint.h>
// Hardware register constants, matching the installed Adafruit header / TI datasheet.
#define ADS1X15_ADDRESS 0x48
#define ADS1X15_REG_POINTER_CONVERT 0
#define ADS1X15_REG_POINTER_CONFIG 1
#define ADS1X15_REG_POINTER_LOWTHRESH 2
#define ADS1X15_REG_POINTER_HITHRESH 3
#define ADS1X15_REG_CONFIG_OS_MASK 0x8000
#define ADS1X15_REG_CONFIG_OS_SINGLE 0x8000
#define ADS1X15_REG_CONFIG_MODE_SINGLE 0x0100
#define ADS1X15_REG_CONFIG_CQUE_1CONV 0
#define RATE_ADS1115_128SPS 0x0080
#define ADS1X15_REG_CONFIG_MUX_DIFF_0_1 0
#define ADS1X15_REG_CONFIG_MUX_DIFF_2_3 0x3000
constexpr uint16_t MUX_BY_CHANNEL[] = {0x4000, 0x5000, 0x6000, 0x7000};
enum adsGain_t { GAIN_ONE = 0x0200 };
class Adafruit_ADS1115 {
  public:
    void setGain(adsGain_t) {}
    float computeVolts(int16_t counts) const { return counts * (4.096f / 32768); }
    // Deliberately no begin() or blocking ADC methods: these calls would fail this host build.
};
