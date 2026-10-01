#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// CSV has no standard representation for an embedded NUL. Reject it rather than silently
// shortening a JSON string at that byte. Commas, quotes, CR and LF use ordinary CSV quoting.
namespace loomCsv {
template <typename Output> bool writeText(Output &output, const char *text, size_t length) {
    if (text == nullptr) {
        return length == 0;
    }
    bool quoted = false;
    for (size_t index = 0; index < length; ++index) {
        const char value = text[index];
        if (value == '\0') {
            return false;
        }
        quoted = quoted || value == ',' || value == '"' || value == '\r' || value == '\n';
    }
    if (quoted && output.write('"') != 1) {
        return false;
    }
    for (size_t index = 0; index < length; ++index) {
        if (text[index] == '"' && output.write('"') != 1) {
            return false;
        }
        if (output.write(static_cast<uint8_t>(text[index])) != 1) {
            return false;
        }
    }
    return !quoted || output.write('"') == 1;
}

template <typename Output> bool writeText(Output &output, const char *text) {
    return writeText(output, text, text ? strlen(text) : 0);
}

// ArduinoJson can serialize directly through this tiny writer. A JSON array/object stays in
// one quoted CSV cell, with its JSON quotes doubled, without a temporary string or row buffer.
template <typename Output> class QuotedJsonWriter {
  public:
    explicit QuotedJsonWriter(Output &output) : output(output) {}
    size_t write(uint8_t value) {
        if (value == '"' && output.write('"') != 1) {
            return 0;
        }
        return output.write(value);
    }
    size_t write(const uint8_t *values, size_t length) {
        size_t written = 0;
        while (written < length && write(values[written]) == 1) {
            ++written;
        }
        return written;
    }

  private:
    Output &output;
};
} // namespace loomCsv
