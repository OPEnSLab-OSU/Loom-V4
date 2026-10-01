// Production streaming verifier; no SD hardware or compiler run is implied by this source.
#include <cassert>
#include <string>
#include "Utilities/Loom_CsvChecksum.h"

const std::string headers = "serial\r\n\r\nidentity,value,checksum\r\n,,checksum\r\n";
std::string row(const std::string &prefix) {
    uint16_t sum = 0;
    for (unsigned char byte : prefix) {
        sum = static_cast<uint16_t>(sum + byte);
    }
    return prefix + std::to_string(sum) + "\r\n";
}
bool verify(const std::string &bytes, uint32_t expectedRows = 1) {
    loomCsv::ChecksumVerifier check;
    for (unsigned char byte : bytes) {
        if (!check.consume(byte)) {
            return false;
        }
    }
    return check.finish() && check.rowCount() == expectedRows;
}
int main() {
    assert(verify(headers, 0));
    assert(verify(headers + row("node,42,")));
    assert(verify(headers + row("node,\"a,b\"\"quoted\"\"\r\nnext line\",")));
    assert(verify(headers + row("node," + std::string(600, 'z') + ","))); // 16-bit wrap.
    assert(verify(headers + row("node,1,") + row("node,2,"), 2));
    std::string damaged = headers + row("node,42,");
    damaged[headers.size() + 5] = '9';
    assert(!verify(damaged));
    assert(!verify(headers + "node,42,65536\r\n"));
    assert(!verify(headers + "node,42,\r\n"));
    assert(!verify(headers + "node,42,0x1234\r\n"));
    assert(!verify(headers + row("node,42,").substr(0, 8))); // Truncated final record.
    assert(!verify(headers + "node,\"unterminated\n"));
    assert(!verify(headers + "node,42,0\rX"));
    assert(!verify(headers + row("node," + std::string(16385, 'z') + ",")));
}
