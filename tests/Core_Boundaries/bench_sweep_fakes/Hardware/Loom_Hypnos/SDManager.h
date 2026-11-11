#pragma once

#include <stdint.h>
#include <map>
#include <sstream>
#include <string>

#define F(text) text

class Print {
  public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t value) = 0;
    virtual size_t write(const uint8_t *data, size_t size) {
        size_t written = 0;
        while (written < size && write(data[written])) ++written;
        return written;
    }
    size_t print(const char *text) {
        const std::string value(text ? text : "");
        return write(reinterpret_cast<const uint8_t *>(value.data()), value.size());
    }
    size_t print(char value) { return write(static_cast<uint8_t>(value)); }
    template <typename Value> size_t print(Value value) {
        std::ostringstream text;
        text << +value;
        return print(text.str().c_str());
    }
    size_t println() { return print("\r\n"); }
    template <typename Value> size_t println(Value value) { return print(value) + println(); }
    int getWriteError() const { return error; }
    void setWriteError(int value = 1) { error = value; }
  private:
    int error = 0;
};

class MemoryOutput : public Print {
  public:
    std::string content;
    using Print::write;
    size_t write(uint8_t value) override { content += static_cast<char>(value); return 1; }
};

class Manager {
  public:
    const char *get_serial_num() const { return "hub-serial"; }
};

class SDManager {
  public:
    SDManager(Manager *, int) {}
    bool begin() { return available; }
    bool fileExists(const char *path) { return files.count(path) != 0; }
    bool writeDebugRecords(const char *path, bool (*writer)(Print &, void *), void *context) {
        if (failWrites) return false;
        MemoryOutput output;
        if (!writer(output, context)) return false;
        files[path] += output.content;
        return true;
    }
    static bool available, failWrites;
    static std::map<std::string, std::string> files;
};
