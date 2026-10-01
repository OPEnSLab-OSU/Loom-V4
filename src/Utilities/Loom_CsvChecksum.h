#pragma once
#include <cstddef>
#include <cstdint>

namespace loomCsv {
////////////////////////////////////////////////////////////////////////////////////////////////////
// Additive-16 checksum over every raw row byte BEFORE the checksum digits, including its last
// comma. CR/LF record endings are excluded. Quotes and newlines inside a quoted cell count.
// This detects accidental damage; it is not an authentication or cryptographic checksum.
class ChecksumVerifier {
  public:
    bool consume(uint8_t byte) {
        if (pendingCr && byte != '\n') {
            return false;
        }
        pendingCr = false;
        ++bytes;
        if (bytes > MAX_RECORD_BYTES) {
            return false;
        }
        if (byte == '"') {
            quoted = !quoted; // Doubled quotes toggle twice; the enclosed comma stays quoted.
        }
        if (!quoted && byte == '\n') {
            const bool valid = records < 4 || (haveComma && digits > 0 && number <= UINT16_MAX &&
                                               number == prefix);
            ++records;
            bytes = 0;
            sum = prefix = 0;
            number = digits = 0;
            haveComma = false;
            return valid;
        }
        if (!quoted && byte == '\r') {
            pendingCr = true;
            return true; // Arduino println emits CR/LF; CR does not enter the row checksum.
        }
        sum = static_cast<uint16_t>(sum + byte);
        if (!quoted && byte == ',') {
            prefix = sum;
            number = digits = 0;
            haveComma = true;
        } else if (haveComma && records >= 4) {
            if (byte >= '0' && byte <= '9' && digits < 5) {
                number = number * 10 + byte - '0';
                ++digits;
            } else {
                number = UINT32_MAX; // Invalid last field; a later comma can start a new one.
                digits = 6;
            }
        }
        return true;
    }
    bool finish() const { return bytes == 0 && !quoted && !pendingCr && records >= 4; }
    uint32_t rowCount() const { return records > 4 ? records - 4 : 0; }

  private:
    static constexpr uint32_t MAX_RECORD_BYTES = 16384;
    uint32_t records = 0, bytes = 0, number = 0, digits = 0;
    uint16_t sum = 0, prefix = 0;
    bool quoted = false, haveComma = false, pendingCr = false;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace loomCsv
