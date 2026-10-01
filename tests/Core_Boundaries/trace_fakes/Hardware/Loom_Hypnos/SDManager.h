#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>

#if defined(_MSC_VER)
#define __attribute__(...)
#define __builtin_return_address(depth) reinterpret_cast<void *>(uintptr_t(0x1001))
#endif
#define F(value) value

namespace traceFake {
static uint32_t ms = 0;
static uint32_t fraction = 0;
static uint32_t interrupt = 0;
static bool hookLinked = true;
} // namespace traceFake
inline uint32_t millis() { return traceFake::ms; }
inline uint32_t micros() { return traceFake::ms * 1000u + traceFake::fraction; }
inline uint32_t __get_IPSR() { return traceFake::interrupt; }

class Print {
  public:
    std::string bytes;
    size_t limit = SIZE_MAX;
    bool error = false;
    virtual ~Print() = default;
    virtual size_t write(uint8_t value) { return write(&value, 1); }
    virtual size_t write(const uint8_t *data, size_t length) {
        const size_t available = bytes.size() < limit ? limit - bytes.size() : 0;
        const size_t accepted = length < available ? length : available;
        bytes.append(reinterpret_cast<const char *>(data), accepted);
        error = error || accepted != length;
        return accepted;
    }
    size_t print(const char *value) {
        return write(reinterpret_cast<const uint8_t *>(value), strlen(value));
    }
    size_t print(uint32_t value) { return print(std::to_string(value).c_str()); }
    size_t print(int32_t value) { return print(std::to_string(value).c_str()); }
    size_t println(char value) { return write(static_cast<uint8_t>(value)) + print("\r\n"); }
    bool getWriteError() const { return error; }
};

class SDManager {
  public:
    std::string saved;
    std::string perfetto;
    bool failPerfetto = false;
    std::string lastPath;
    std::set<std::string> existing;
    size_t limit = SIZE_MAX;
    unsigned int batches = 0;
    bool ready = true;
    int sessionNumber = 0;
    int getDebugFileNumber() const { return sessionNumber; }
    bool canWriteDebugLogs() const { return ready; }
    bool fileExists(const char *path) { return existing.count(path) != 0; }
    bool writeLineToFile(const char *path, const char *line) {
        if (!ready) { return false; }
        lastPath = path;
        if (std::string(path).find(".perfetto.json") != std::string::npos) {
            perfetto = std::string(line) + "\r\n";
        } else {
            saved += std::string(line) + "\r\n";
        }
        existing.insert(path);
        return true;
    }
    bool appendDebugTrace(const char *, bool (*writer)(Print &, void *), void *context) {
        Print output;
        output.limit = limit;
        if (!ready || failPerfetto || !writer(output, context) || output.error) { return false; }
        assert(perfetto.size() >= 4);
        perfetto.resize(perfetto.size() - 4);
        perfetto += output.bytes + "]}\r\n";
        return true;
    }
    bool writeDebugRecords(const char *path, bool (*writer)(Print &, void *), void *context) {
        ++batches;
        lastPath = path;
        Print output;
        output.limit = limit;
        const bool ok = ready && writer(output, context) && !output.error;
        if (ok) { saved += output.bytes; }
        return ok;
    }
};
