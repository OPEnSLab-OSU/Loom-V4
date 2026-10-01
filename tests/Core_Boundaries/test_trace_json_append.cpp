#include "../../src/Utilities/Loom_SDUtils.h"
#include <cassert>
#include <iostream>
#include <string>
#include <algorithm>

struct File {
    std::string bytes = "{\"traceEvents\":[{}]}\r\n";
    size_t position = 0;
    bool error = false;
    bool failWriteOnce = false;
    bool failWrites = false;
    bool failSyncOnce = false;
    bool failSyncs = false;
    bool failTruncate = false;
    bool failClose = false;
    bool failRead = false;
    unsigned int closes = 0;
    uint32_t fileSize() const { return static_cast<uint32_t>(bytes.size()); }
    bool seekSet(size_t offset) { position = offset; return offset <= bytes.size(); }
    int read(char *destination, int count) {
        if (failRead || position + count > bytes.size()) { return -1; }
        bytes.copy(destination, count, position);
        position += count;
        return count;
    }
    size_t write(const char *source, size_t count) {
        const size_t accepted = failWrites ? 0 : failWriteOnce ? std::min(size_t(2), count) : count;
        failWriteOnce = false;
        if (bytes.size() < position + accepted) { bytes.resize(position + accepted); }
        bytes.replace(position, accepted, source, accepted);
        position += accepted;
        error = error || accepted != count;
        return accepted;
    }
    bool truncate(size_t size) { if (failTruncate) { return false; } bytes.resize(size); return true; }
    bool sync() { const bool ok = !failSyncOnce && !failSyncs; failSyncOnce = false; return ok; }
    bool close() { ++closes; return !failClose; }
    bool getWriteError() const { return error; }
    void clearWriteError() { error = false; }
};

int main() {
    const auto writer = [](File &file) { return file.write(",{}", 3) == 3; };
    {
        File file;
        const auto result = loomSD::appendTraceEvents(file, writer);
        assert(result.status == SDWriteStatus::Saved && result.committed);
        assert(file.bytes == "{\"traceEvents\":[{},{}]}\r\n" && file.closes == 1);
        assert(loomSD::appendTraceEvents(file, writer).status == SDWriteStatus::Saved);
        assert(file.bytes == "{\"traceEvents\":[{},{},{}]}\r\n");
    }
    for (int fault = 0; fault < 2; ++fault) {
        File file;
        const std::string before = file.bytes;
        file.failWriteOnce = fault == 0;
        file.failSyncOnce = fault == 1;
        const auto result = loomSD::appendTraceEvents(file, writer);
        assert(result.status == SDWriteStatus::Failed && !result.committed);
        assert(file.bytes == before && file.closes == 1);
    }
    for (int fault = 0; fault < 4; ++fault) {
        File file;
        file.failWriteOnce = true;
        file.failTruncate = fault == 0;
        file.failWrites = fault == 1;
        file.failSyncs = fault == 2;
        file.failClose = fault == 3;
        assert(loomSD::appendTraceEvents(file, writer).status == SDWriteStatus::Uncertain);
        assert(file.closes == 1);
    }
    {
        File file;
        file.failClose = true;
        const auto result = loomSD::appendTraceEvents(file, writer);
        assert(result.committed && result.status == SDWriteStatus::Uncertain);
    }
    for (int fault = 0; fault < 3; ++fault) {
        File file;
        if (fault == 0) { file.bytes = "damaged"; }
        if (fault == 1) { file.bytes = ""; }
        if (fault == 2) { file.failRead = true; }
        const std::string before = file.bytes;
        assert(loomSD::appendTraceEvents(file, writer).status == SDWriteStatus::Uncertain);
        assert(file.bytes == before && file.closes == 1);
    }
    std::cout << "PASS closed Chrome JSON batches, trailer restoration, short writes, sync/close faults, damaged tails\n";
}
