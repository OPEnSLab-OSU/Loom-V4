// Deferred regression cases: no compiler was run during the source pass.
#include "../../src/Utilities/Loom_CsvUtils.h"
#include <assert.h>
#include <string>

struct Output {
    std::string bytes;
    size_t failAfter = SIZE_MAX;
    size_t write(uint8_t value) {
        if (bytes.size() >= failAfter) {
            return 0;
        }
        bytes += static_cast<char>(value);
        return 1;
    }
};

int main() {
    Output plain;
    assert(loomCsv::writeText(plain, "temperature"));
    assert(plain.bytes == "temperature"); // Ordinary existing cells keep their exact bytes.
    Output quoted;
    assert(loomCsv::writeText(quoted, "a,\"b\"\r\nc"));
    assert(quoted.bytes == "\"a,\"\"b\"\"\r\nc\"");
    Output empty;
    assert(loomCsv::writeText(empty, nullptr, 0));
    assert(!loomCsv::writeText(empty, nullptr, 1));
    const char withNull[] = {'a', '\0', 'b'};
    assert(!loomCsv::writeText(empty, withNull, sizeof(withNull)));
    assert(empty.bytes.empty()); // Reject before any field bytes, never silently shorten it.
    for (size_t failure = 0; failure < quoted.bytes.size(); ++failure) {
        Output shortWrite;
        shortWrite.failAfter = failure;
        assert(!loomCsv::writeText(shortWrite, "a,\"b\"\r\nc"));
        assert(shortWrite.bytes.size() == failure);
    }
    Output nested;
    loomCsv::QuotedJsonWriter<Output> writer(nested);
    const uint8_t json[] = "{\"a\":[1,2]}";
    assert(writer.write(json, sizeof(json) - 1) == sizeof(json) - 1);
    assert(nested.bytes == "{\"\"a\"\":[1,2]}");
    for (size_t failure = 0; failure < nested.bytes.size(); ++failure) {
        Output shortWrite;
        shortWrite.failAfter = failure;
        loomCsv::QuotedJsonWriter<Output> failingWriter(shortWrite);
        assert(failingWriter.write(json, sizeof(json) - 1) < sizeof(json) - 1);
    }
    return 0;
}
