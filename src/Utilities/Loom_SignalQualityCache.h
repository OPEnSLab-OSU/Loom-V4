#pragma once
#include <cstdint>

namespace loomSignal {
// Raw AT+CSQ RSSI (0..31); 99 and transport failures are not signal readings.
// RAM only: a fresh boot never inherits another session's cached signal.
class QualityCache {
  public:
    bool observe(int value) {
        if (value < 0 || value > 31) return false;
        quality = static_cast<int8_t>(value); return true;
    }
    bool hasValue() const { return quality >= 0; }
    int value() const { return quality; }
  private:
    int8_t quality = -1;
};
}
